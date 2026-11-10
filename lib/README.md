# Libraries for POW_PZEM016_1P

This folder is reserved for local library code supporting the `POW_PZEM016_1P` project.

## Purpose

- Store custom or reusable modules used by the power monitoring firmware.
- Keep the `src/` directory clean and focused on the application flow.

## Usage

- Add each library as a separate folder inside `lib/`.
- PlatformIO automatically includes local libraries during the build process.

## Notes

- External dependencies should be managed in `platformio.ini` using `lib_deps`.
- Use this folder for library code that is shared across multiple source files.
