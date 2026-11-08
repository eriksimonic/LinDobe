# Extended directory notification entry point

`ex.c` looks up `ReadDirectoryChangesExW` dynamically so the unpatched runtime
fails cleanly when the export is missing, instead of aborting on a missing
import as Animation Composer does.

```sh
x86_64-w64-mingw32-gcc -O1 -o directory-notify.exe ex.c
```

For a local Wine development test:

```sh
/opt/adobewine/runtime/bin/winegcc -m64 -Wall -Wextra -o /tmp/directory-notify.exe ex.c
```

Use an explicit test prefix and a timeout. The test creates a temporary
directory, queues a basic asynchronous notification, creates a file, and
checks the returned action and UTF-16 filename. It also checks rejection of
an invalid information class. Temporary files and handles are cleaned up.

For extended information, the test accepts either a queued request (cancelled
before continuing) or `ERROR_INVALID_FUNCTION`, which represents unsupported
extended notifications. It never interprets basic records as extended records.

The unpatched LinDobe Wine 11.20 runtime reports a missing export. With patch
0135, basic notifications pass and extended notifications fail normally,
allowing Animation Composer 4.2.6 to try its existing `ReadDirectoryChangesW`
fallback. Full extended-notification support and Windows reference testing
are not claimed by this test.
