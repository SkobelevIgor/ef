---
description: Create a new release for the project.
---

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty).

There is required `Version` attribute. If user didn't add this, re-ask user.

Should create a new release with provided version from $ARGUMENTS.
The release should be tagged in the version control system and include any relevant release notes or changelog entries.
Release should contain release notes which is diff between current release and previous one. If there is no previous release, include all changes since the beginning of the project.