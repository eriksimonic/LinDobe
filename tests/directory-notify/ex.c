#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef BOOL (WINAPI *read_changes_ex)(HANDLE, void *, DWORD, BOOL, DWORD,
        DWORD *, OVERLAPPED *, LPOVERLAPPED_COMPLETION_ROUTINE, DWORD);

int main(void)
{
    read_changes_ex read_ex = (void *)GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "ReadDirectoryChangesExW");
    WCHAR root[MAX_PATH], directory[MAX_PATH], filename[MAX_PATH];
    OVERLAPPED ov = {0};
    HANDLE handle = INVALID_HANDLE_VALUE, file = INVALID_HANDLE_VALUE;
    union { FILE_NOTIFY_INFORMATION info; BYTE bytes[4096]; } buffer;
    DWORD bytes, error;
    BOOL result;
    int failed = 1;

    if (!read_ex)
    {
        puts("ReadDirectoryChangesExW export missing: FAIL");
        return 1;
    }
    if (!GetTempPathW(MAX_PATH, root)) return 1;
    if (lstrlenW(root) + 64 >= MAX_PATH) return 1;
    wsprintfW(directory, L"%snotify-ex-%lu", root, GetCurrentProcessId());
    wsprintfW(filename, L"%s\\created.tmp", directory);
    if (!CreateDirectoryW(directory, NULL)) return 1;
    handle = CreateFileW(directory, FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (handle == INVALID_HANDLE_VALUE || !ov.hEvent) goto done;

    SetLastError(0);
    result = read_ex(handle, &buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL, 0);
    if (result || GetLastError() != ERROR_INVALID_PARAMETER)
    {
        puts("Invalid information class: FAIL");
        goto done;
    }

    SetLastError(0);
    result = read_ex(handle, &buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL, 2);
    error = GetLastError();
    if (result)
    {
        CancelIoEx(handle, &ov);
        if (WaitForSingleObject(ov.hEvent, 3000) != WAIT_OBJECT_0) goto done;
        GetOverlappedResult(handle, &ov, &bytes, FALSE);
        puts("Extended notifications accepted; probe cancelled");
    }
    else if (error == ERROR_INVALID_FUNCTION)
        puts("Extended notifications explicitly unsupported: PASS");
    else
    {
        printf("Extended notifications unexpected error %lu: FAIL\n", (unsigned long)error);
        goto done;
    }

    ResetEvent(ov.hEvent);
    ov.Internal = ov.InternalHigh = ov.Offset = ov.OffsetHigh = 0;
    result = read_ex(handle, &buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL, 1);
    if (!result)
    {
        printf("Queue basic notifications error %lu: FAIL\n", (unsigned long)GetLastError());
        goto done;
    }
    file = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_READ,
            NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
    if (WaitForSingleObject(ov.hEvent, 3000) != WAIT_OBJECT_0)
    {
        puts("Basic notification timed out: FAIL");
        goto done;
    }
    if (!GetOverlappedResult(handle, &ov, &bytes, FALSE)) goto done;
    if (bytes < FIELD_OFFSET(FILE_NOTIFY_INFORMATION, FileName)
            || buffer.info.Action != FILE_ACTION_ADDED
            || buffer.info.FileNameLength != sizeof(L"created.tmp") - sizeof(WCHAR)
            || memcmp(buffer.info.FileName, L"created.tmp", buffer.info.FileNameLength))
    {
        puts("Basic notification contents: FAIL");
        goto done;
    }
    puts("Basic asynchronous file creation notification: PASS");
    failed = 0;

done:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (handle != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(handle, NULL);
        GetOverlappedResult(handle, &ov, &bytes, TRUE);
        CloseHandle(handle);
    }
    if (ov.hEvent) CloseHandle(ov.hEvent);
    DeleteFileW(filename);
    RemoveDirectoryW(directory);
    return failed;
}
