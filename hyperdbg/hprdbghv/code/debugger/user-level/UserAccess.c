/**
 * @file UserAccess.c
 * @author Sina Karvandi (sina@rayanfam.com)
 * @brief Access and parse user-mode components of binaries
 * @details Access to Portable Executables
 *
 * @version 0.1
 * @date 2021-12-24
 * 
 * @copyright This project is released under the GNU Public License v3.
 * 
 */
#include "..\hprdbghv\pch.h"

/**
 * @brief Get the image path from process Id
 * @details This function should be called in vmx non-root
 * for size 512 is enough, if the size is not enough it 
 * returns FALSE
 * it's up to the user to deallocate ProcessImageName.Buffer
 * 
 * @param ProcessId 
 * @param ProcessImageName 
 * @param SizeOfImageNameToBeAllocated 
 * @return BOOLEAN 
 */
BOOLEAN
UserAccessAllocateAndGetImagePathFromProcessId(HANDLE          ProcessId,
                                               PUNICODE_STRING ProcessImageName,
                                               UINT32          SizeOfImageNameToBeAllocated)
{
    NTSTATUS        Status;
    ULONG           ReturnedLength;
    ULONG           BufferLength;
    HANDLE          ProcessHandle;
    PVOID           Buffer;
    PEPROCESS       EProcess;
    PUNICODE_STRING ImageName;

    //
    // This eliminates the possibility of the IDLE Thread/Process
    //
    PAGED_CODE();

    Status = PsLookupProcessByProcessId(ProcessId, &EProcess);

    if (NT_SUCCESS(Status))
    {
        Status = ObOpenObjectByPointer(EProcess, 0, NULL, 0, 0, KernelMode, &ProcessHandle);

        if (!NT_SUCCESS(Status))
        {
            LogError("Err, cannot get the process object (%08x)", Status);
            return FALSE;
        }

        ObDereferenceObject(EProcess);
    }
    else
    {
        //
        // Probably, the process id is wrong!
        //
        return FALSE;
    }

    if (g_ZwQueryInformationProcess == NULL)
    {
        return FALSE;
    }

    //
    // Query the actual size of the process path
    //
    Status = g_ZwQueryInformationProcess(ProcessHandle,
                                         ProcessImageFileName,
                                         NULL, // Buffer
                                         0,    // Buffer size
                                         &ReturnedLength);

    if (Status != STATUS_INFO_LENGTH_MISMATCH)
    {
        //
        // ZwQueryInformationProcess failed
        //
        return FALSE;
    }

    //
    // Check there is enough space to store the actual process path when it is found
    // If not return FALSE
    //
    BufferLength = ReturnedLength - sizeof(UNICODE_STRING);

    if (SizeOfImageNameToBeAllocated < BufferLength)
    {
        return FALSE;
    }

    //
    // Allocate a temporary buffer to store the path name
    //
    Buffer = ExAllocatePoolWithTag(NonPagedPool, ReturnedLength, POOLTAG);

    if (Buffer == NULL)
    {
        return FALSE;
    }

    //
    // Retrieve the process path from the handle to the process
    //
    Status = g_ZwQueryInformationProcess(ProcessHandle,
                                         ProcessImageFileName,
                                         Buffer,
                                         ReturnedLength,
                                         &ReturnedLength);

    if (NT_SUCCESS(Status))
    {
        //
        // Copy the path name
        //
        ImageName = (PUNICODE_STRING)Buffer;

        //
        // Alloate UNICODE_STRING
        //
        ProcessImageName->Length        = 0;
        ProcessImageName->MaximumLength = SizeOfImageNameToBeAllocated;
        ProcessImageName->Buffer        = (PWSTR)ExAllocatePoolWithTag(NonPagedPool, SizeOfImageNameToBeAllocated, POOLTAG);

        if (ProcessImageName->Buffer == NULL)
        {
            return FALSE;
        }

        RtlZeroMemory(ProcessImageName->Buffer, SizeOfImageNameToBeAllocated);

        //
        // Copy path to the buffer
        //
        RtlCopyUnicodeString(ProcessImageName, ImageName);

        //
        // Free the temp buffer which stored the path
        //
        ExFreePoolWithTag(Buffer, POOLTAG);

        return TRUE;
    }
    else
    {
        //
        // There was an error in ZwQueryInformationProcess
        // Free the temp buffer which stored the path
        //
        ExFreePoolWithTag(Buffer, POOLTAG);
        return FALSE;
    }
}

/**
 * @brief Get the process's PEB from process Id
 * @details This function should be called in vmx non-root
 * 
 * @param ProcessId 
 * @param Peb 
 * @return BOOLEAN 
 */
BOOLEAN
UserAccessGetPebFromProcessId(HANDLE ProcessId, PUINT64 Peb)
{
    NTSTATUS                  Status;
    ULONG                     ReturnedLength;
    HANDLE                    ProcessHandle;
    PEPROCESS                 EProcess;
    PPEB                      ProcessPeb;
    PROCESS_BASIC_INFORMATION ProcessBasicInfo = {0};

    //
    // This eliminates the possibility of the IDLE Thread/Process
    //
    PAGED_CODE();

    Status = PsLookupProcessByProcessId(ProcessId, &EProcess);

    if (NT_SUCCESS(Status))
    {
        Status = ObOpenObjectByPointer(EProcess, 0, NULL, 0, 0, KernelMode, &ProcessHandle);

        if (!NT_SUCCESS(Status))
        {
            LogError("Err, cannot get the process object (%08x)", Status);
            return FALSE;
        }

        ObDereferenceObject(EProcess);
    }
    else
    {
        //
        // Probably, the process id is wrong!
        //
        return FALSE;
    }

    if (g_ZwQueryInformationProcess == NULL)
    {
        return FALSE;
    }

    //
    //  Retrieve the process path from the handle to the process
    //
    Status = g_ZwQueryInformationProcess(ProcessHandle,
                                         ProcessBasicInformation,
                                         &ProcessBasicInfo,
                                         sizeof(PROCESS_BASIC_INFORMATION),
                                         &ReturnedLength);

    if (NT_SUCCESS(Status))
    {
        ProcessPeb = ProcessBasicInfo.PebBaseAddress;

        *Peb = ProcessPeb;
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Print loaded modules details from PEB
 * @details This function should be called in vmx non-root
 * 
 * @param Proc 
 * @return BOOLEAN 
 */
BOOLEAN
UserAccessPrintLoadedModulesX64(PEPROCESS Proc)
{
    KAPC_STATE     State;
    UNICODE_STRING Name;
    PPEB           Peb = NULL;
    PPEB_LDR_DATA  Ldr = NULL;

    if (g_PsGetProcessPeb == NULL)
    {
        return FALSE;
    }

    //
    // Process PEB, function is unexported and undocumented
    //
    Peb = (PPEB)g_PsGetProcessPeb(Proc);

    if (!Peb)
    {
        return FALSE;
    }

    KeStackAttachProcess(Proc, &State);

    Ldr = (PPEB_LDR_DATA)Peb->Ldr;

    if (!Ldr)
    {
        KeUnstackDetachProcess(&State);
        return FALSE;
    }

    //
    // loop the linked list
    //
    for (PLIST_ENTRY List = (PLIST_ENTRY)Ldr->ModuleListLoadOrder.Flink;
         List != &Ldr->ModuleListLoadOrder;
         List = (PLIST_ENTRY)List->Flink)
    {
        PLDR_DATA_TABLE_ENTRY Entry =
            CONTAINING_RECORD(List, LDR_DATA_TABLE_ENTRY, InLoadOrderModuleList);

        Log("Base: %016llx\tEntryPoint: %016llx\tModule: %ws\tPath: %ws\n",
            Entry->DllBase,
            Entry->EntryPoint,
            Entry->BaseDllName.Buffer,
            Entry->FullDllName.Buffer);
    }

    KeUnstackDetachProcess(&State);

    return TRUE;
}

/**
 * @brief Print loaded modules details from PEB
 * @details This function should be called in vmx non-root
 * 
 * @param Proc 
 * @return BOOLEAN 
 */
BOOLEAN
UserAccessPrintLoadedModulesX86(PEPROCESS Proc)
{
    KAPC_STATE      State;
    UNICODE_STRING  Name;
    PPEB32          Peb = NULL;
    PPEB_LDR_DATA32 Ldr = NULL;

    if (g_PsGetProcessWow64Process == NULL)
    {
        return FALSE;
    }

    //
    // get process PEB for the x86 part, function is unexported and undocumented
    //
    Peb = (PPEB32)g_PsGetProcessWow64Process(Proc);

    if (!Peb)
    {
        return FALSE;
    }

    KeStackAttachProcess(Proc, &State);

    Ldr = (PPEB_LDR_DATA32)Peb->Ldr;

    if (!Ldr)
    {
        KeUnstackDetachProcess(&State);
        return FALSE;
    }

    //
    // loop the linked list
    //
    for (PLIST_ENTRY32 List = (PLIST_ENTRY32)Ldr->InLoadOrderModuleList.Flink;
         List != &Ldr->InLoadOrderModuleList;
         List = (PLIST_ENTRY32)List->Flink)
    {
        PLDR_DATA_TABLE_ENTRY32 Entry =
            CONTAINING_RECORD(List, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

        //
        // since the PEB is x86, the DLL is x86, and so the base address is in x86 (4 byte as compared to 8 byte)
        // and the UNICODE STRING is in 32 bit(UNICODE_STRING32), and because there is no viable conversion
        // we are just going to force everything in
        //
        UNICODE_STRING ModuleName;
        UNICODE_STRING ModulePath;
        UINT64         BaseAddr          = NULL;
        UINT64         EntrypointAddress = NULL;

        BaseAddr          = Entry->DllBase;
        EntrypointAddress = Entry->EntryPoint;

        ModuleName.Length        = Entry->BaseDllName.Length;
        ModuleName.MaximumLength = Entry->BaseDllName.MaximumLength;
        ModuleName.Buffer        = (PWCH)Entry->BaseDllName.Buffer;

        ModulePath.Length        = Entry->FullDllName.Length;
        ModulePath.MaximumLength = Entry->FullDllName.MaximumLength;
        ModulePath.Buffer        = (PWCH)Entry->FullDllName.Buffer;

        Log("Base: %016llx\tEntryPoint: %016llx\tModule: %ws\tPath: %ws\n",
            BaseAddr,
            EntrypointAddress,
            ModuleName.Buffer,
            ModulePath.Buffer);
    }

    KeUnstackDetachProcess(&State);

    return TRUE;
}

/**
 * @brief Detects whether process is 32-bit or 64-bit
 * @details This function should be called in vmx non-root
 * 
 * @param ProcessId 
 * @param Is32Bit 
 * 
 * @return BOOLEAN 
 */
BOOLEAN
UserAccessIsWow64Process(HANDLE ProcessId, PBOOLEAN Is32Bit)
{
    PEPROCESS  SourceProcess;
    KAPC_STATE State = {0};

    if (PsLookupProcessByProcessId(ProcessId, &SourceProcess) != STATUS_SUCCESS)
    {
        //
        // if the process not found
        //
        return FALSE;
    }

    ObDereferenceObject(SourceProcess);

    if (g_PsGetProcessWow64Process == NULL || g_PsGetProcessPeb == NULL)
    {
        return FALSE;
    }

    if (g_PsGetProcessWow64Process(SourceProcess))
    {
        //
        // x86 process, walk x86 module list
        //

        *Is32Bit = TRUE;

        return TRUE;
    }
    else if (g_PsGetProcessPeb(SourceProcess))
    {
        //
        // x64 process, walk x64 module list
        //
        *Is32Bit = FALSE;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/**
 * @brief Prints loaded modules
 * @details This function should be called in vmx non-root
 * 
 * @param ProcessId 
 * 
 * @return BOOLEAN 
 */
BOOLEAN
UserAccessPrintLoadedModules(HANDLE ProcessId)

{
    PEPROCESS SourceProcess;
    BOOLEAN   Is32Bit;

    if (PsLookupProcessByProcessId(ProcessId, &SourceProcess) != STATUS_SUCCESS)
    {
        //
        // if the process not found
        //
        return FALSE;
    }

    ObDereferenceObject(SourceProcess);

    //
    // check whether the target process is 32-bit or 64-bit
    //
    if (!UserAccessIsWow64Process(ProcessId, &Is32Bit))
    {
        //
        // Unable to detect whether it's 32-bit or 64-bit
        //
        return FALSE;
    }

    if (Is32Bit)
    {
        //
        // x86 process, walk x86 module list
        //
        if (UserAccessPrintLoadedModulesX86(SourceProcess))
        {
            return TRUE;
        }
    }
    else
    {
        //
        // x64 process, walk x64 module list
        //
        if (UserAccessPrintLoadedModulesX64(SourceProcess))
        {
            return TRUE;
        }
    }

    return FALSE;
}
