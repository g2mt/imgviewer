# Plan: Make PDFs within zip archives work

## Problem

When `USE_QT_PDF` is on, `.pdf` files at the top-level filesystem work:
`Filter::requestDirectoryEntries()` detects the `.pdf` extension on the current
URL and calls `requestPdfEntries()`, which loads the document and emits
`PdfDirectoryEntry` items (one per page). The image list renders them as
selectable thumbnails, and clicking one feeds `ImageView::setPdfPage()`.

Inside a zip archive, `.pdf` files are currently classified as `Archive`
entries (`isArchiveSuffix` includes `"pdf"` when `USE_QT_PDF` is defined).
Clicking one tries to extract it as a nested archive, which fails.

## Design

Give `.pdf` files their own `EntryType::Pdf` so they appear as navigable
entries in the directory tree. Navigating into one sets the current URL to the
PDF file path and triggers `requestDirectoryEntries()`, which already knows how
to produce `PdfDirectoryEntry` pages from a local `.pdf` file.

For **libarchive** this is straightforward: the zip is fully extracted to a
temp directory, so the PDF sits on the local filesystem and the existing
`m_currentUrl.isLocalFile()` / `.pdf` check works.

For **KIO** the PDF lives behind a `zip://` URL. We need a small additional
path in `requestDirectoryEntries()` that detects a `.pdf` inside a zip scheme,
downloads the bytes via `KIO::storedGet`, writes them to a `QTemporaryFile`,
and loads that local copy with `QPdfDocument`.

---

## Tasks

### 1. `DirectoryEntry.h` – new entry type `Pdf`

- Add `Pdf` after `Archive` in `BaseDirectoryEntry::EntryType`.

### 2. `DirectoryEntry.cpp` – stop classifying `.pdf` as an archive

- In `isArchiveSuffix()`: remove the `#ifdef USE_QT_PDF` block that returns
  `true` for `"pdf"`.  (When `USE_QT_PDF` is *off*, pdf remains uncategorised
  and will be skipped, which is fine.)
- In `fromFileInfo()`: when `USE_QT_PDF` is defined, check `ext == "pdf"`
  **before** the archive check and set `EntryType::Pdf`.

### 3. `Filter.h` – store parent URL + temp file for PDF navigation

- Add `QUrl m_pdfParentUrl` (guarded by `USE_QT_PDF`).  Before navigating into
  a PDF, save `m_currentUrl.resolved(QUrl(".."))` here.  When the user
  navigates up from PDF pages, restore `m_currentUrl = m_pdfParentUrl` and
  clear it.  This works for both libarchive and KIO – no URL guessing.
- Add `#include <QTemporaryFile>` and `std::unique_ptr<QTemporaryFile>
  m_pdfTempFile` (guarded by `#if defined(USE_KIO) && defined(USE_QT_PDF)`)
  for the KIO downloaded-PDF case.

### 4. `Filter.cpp` – wire up PDF navigation & loading

- **`navigateDirectory()`**: treat `EntryType::Pdf` entries like
  directory/archive entries (navigatable).  Save `m_currentUrl.resolved(QUrl(".."))`
  into `m_pdfParentUrl`, set `m_currentUrl` to the entry's URL, emit
  `changed()`.  (Under libarchive the URL is already a local file; under KIO
  it is a `zip://…` URL.)

  In the `UpDirectoryEntry` handler: **before** the existing archive/KIO/local
  logic, check if `m_pdfParentUrl` is not empty.  If set, restore
  `m_currentUrl = m_pdfParentUrl`, clear `m_pdfParentUrl` (and `m_pdfTempFile`
  if KIO), emit `changed()`, and return.  This short-circuits all the fragile
  `..` / scheme arithmetic.

- **`requestDirectoryEntries()`**: keep the existing `m_currentUrl.isLocalFile()
  && .pdf` check.  Add a second branch (guarded by `USE_KIO && USE_QT_PDF`)
  that handles the case where `m_currentUrl.scheme() == "zip"` and the file
  name ends with `.pdf`:
  1. Cancel any in-flight KIO job.
  2. `storedGet` the PDF bytes.
  3. Write to a new `QTemporaryFile` kept alive in `m_pdfTempFile`.
  4. Call `requestPdfEntries(m_pdfTempFile->fileName())`.

- **`requestPdfEntries()`**: generalise so it accepts an explicit file path
  instead of always reading `m_currentUrl.toLocalFile()`.  Keep the existing
  no-arg signature that does `load(m_currentUrl.toLocalFile())` and forward to
  the new one.

### 5. `DirectoryList.cpp` – show PDF entries as navigable items

- In `populate()`: add `EntryType::Pdf` to the `if` condition alongside `Dir`
  and `Archive`.  Use a PDF icon (e.g. `"application-pdf"`).

### 6. `ImageDetailModel.cpp` – ensure PDF page entries flow through

- `FilePathRole` already returns a `PdfDirectoryEntry*` for page entries.
- `populate()` already skips `Dir` but keeps `PdfDirectoryEntry` (via the
  early `qobject_cast` check).  These items come from the filter’s directory
  entries, which after entering a PDF will be the per-page entries.

No changes expected here unless the model’s filtering inadvertently hides
PdfDirectoryEntry items – verify and fix if needed.

### 7. Smoke test

- Libarchive: open a `.zip` containing a `.pdf` → click the PDF in the
  directory tree → pages appear in the image list → click a page → it renders
  in the image view.  Click `..` → returns to the directory containing the PDF
  (archive root, or the subdirectory the PDF lived in).  Click `..` again →
  returns further up as normal.

- KIO: same flow via `zip://` (remote or local zip).  Ensure the downloaded
  temp PDF is cleaned up when navigating away from the PDF.