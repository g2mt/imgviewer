# Coding conventions

- Use 2 spaces for indenting.
- When commiting, do not add anything to the message body. Instead, summarize what the commit does within only the commit title. Use the conventional commit format for the title: `type: description`.
- Prioritize simple code with minimal dependencies.
- Add comments ONLY if it's not immediately obvious from a cursory glance of the code.
- Store header files in `src/include`
- The KF6KIO library, or alternatively libarchive, is used for loading images from zip archives. The compile flags `USE_KIO` and `USE_LIBARCHIVE` will be defined respectively.
