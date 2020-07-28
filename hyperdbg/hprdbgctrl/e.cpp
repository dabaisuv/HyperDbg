/**
 * @file e.cpp
 * @author Sina Karvandi (sina@rayanfam.com)
 * @brief e* command
 * @details
 * @version 0.1
 * @date 2020-07-27
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

VOID CommandEditMemoryHelp() {
  ShowMessages("eb !eb ed !ed eq !eq : edit the memory at specific address \n");
  ShowMessages("e[b]  Byte and ASCII characters\n");
  ShowMessages("e[d]  Double-word values (4 bytes)\n");
  ShowMessages("e[q]  Quad-word values (8 bytes). \n");
  ShowMessages("\n If you want to edit physical (address) memory then add '!' "
               "at the start of the command\n");

  ShowMessages("syntax : \t[!]e[b|d|q] [address] [new memory contents (hex)] "
               "pid [process id (hex)]\n");
  ShowMessages("\t\te.g : eb fffff8077356f010 90 \n");
  ShowMessages("\t\te.g : eb fffff8077356f010 90 90 90 90 \n");
  ShowMessages("\t\te.g : !eq 100000 9090909090909090\n");
  ShowMessages("\t\te.g : !eq 100000 9090909090909090 9090909090909090 "
               "9090909090909090 9090909090909090 9090909090909090\n");
}

VOID CommandEditMemory(vector<string> SplittedCommand) {

  BOOL Status;
  BOOL SetAddress = FALSE;
  BOOL SetValue = FALSE;
  BOOL SetProcId = FALSE;
  BOOL NextIsProcId = FALSE;
  DEBUGGER_EDIT_MEMORY EditMemoryRequest = {0};
  UINT64 Address;
  UINT64 Value = 0;
  UINT32 ProcId = GetCurrentProcessId();
  UINT32 CountOfValues = 0;
  UINT32 FinalSize = 0;
  UINT64 *FinalBuffer;
  vector<UINT64> ValuesToEdit;

  if (SplittedCommand.size() <= 2) {
    ShowMessages("incorrect use of 'e*'\n\n");
    CommandEditMemoryHelp();
    return;
  }

  for (auto Section : SplittedCommand) {

    if (!Section.compare(SplittedCommand.at(0))) {

      if (!Section.compare("!eb")) {
        EditMemoryRequest.MemoryType = EDIT_PHYSICAL_MEMORY;
        EditMemoryRequest.ByteSize = EDIT_BYTE;
      } else if (!Section.compare("!ed")) {
        EditMemoryRequest.MemoryType = EDIT_PHYSICAL_MEMORY;
        EditMemoryRequest.ByteSize = EDIT_DWORD;
      } else if (!Section.compare("!eq")) {
        EditMemoryRequest.MemoryType = EDIT_PHYSICAL_MEMORY;
        EditMemoryRequest.ByteSize = EDIT_QWORD;
      } else if (!Section.compare("eb")) {
        EditMemoryRequest.MemoryType = EDIT_VIRTUAL_MEMORY;
        EditMemoryRequest.ByteSize = EDIT_BYTE;
      } else if (!Section.compare("ed")) {
        EditMemoryRequest.MemoryType = EDIT_VIRTUAL_MEMORY;
        EditMemoryRequest.ByteSize = EDIT_DWORD;
      } else if (!Section.compare("eq")) {
        EditMemoryRequest.MemoryType = EDIT_VIRTUAL_MEMORY;
        EditMemoryRequest.ByteSize = EDIT_QWORD;
      } else {
        //
        // What's this? :(
        //
        ShowMessages("unknown error happened !\n\n");
        CommandEditMemoryHelp();
        return;
      }

      continue;
    }
    if (NextIsProcId) {

      //
      // It's a process id
      //
      NextIsProcId = FALSE;

      if (!ConvertStringToUInt32(Section, &ProcId)) {
        ShowMessages("please specify a correct hex prcoess id\n\n");
        CommandEditMemoryHelp();
        return;
      } else {
        //
        // Means that the proc id is set, next we should read value
        //
        continue;
      }
    }
    //
    // Check if it's a process id or not
    //
    if (!SetProcId && !Section.compare("pid")) {
      NextIsProcId = TRUE;
      continue;
    }

    if (!SetAddress) {
      if (!ConvertStringToUInt64(Section, &Address)) {
        ShowMessages("please specify a correct hex address to edit\n\n");
        CommandEditMemoryHelp();
        return;
      } else {
        //
        // Means that the address is set, next we should read value
        //
        SetAddress = TRUE;
        continue;
      }
    }

    if (SetAddress) {

      //
      // Remove the hex notations
      //
      if (Section.rfind("0x", 0) == 0 || Section.rfind("0X", 0) == 0 ||
          Section.rfind("\\x", 0) == 0 || Section.rfind("\\X", 0) == 0) {
        Section = Section.erase(0, 2);
      } else if (Section.rfind("x", 0) == 0 || Section.rfind("X", 0) == 0) {
        Section = Section.erase(0, 1);
      }
      Section.erase(remove(Section.begin(), Section.end(), '`'), Section.end());

      //
      // Check if the value is valid based on byte counts
      //
      if (EditMemoryRequest.ByteSize == EDIT_BYTE && Section.size() >= 3) {
        ShowMessages("please specify a byte (hex) value for 'eb' or '!eb'\n\n");
        return;
      }
      if (EditMemoryRequest.ByteSize == EDIT_DWORD && Section.size() >= 9) {
        ShowMessages(
            "please specify a dword (hex) value for 'ed' or '!ed'\n\n");
        return;
      }
      if (EditMemoryRequest.ByteSize == EDIT_QWORD && Section.size() >= 17) {
        ShowMessages(
            "please specify a qword (hex) value for 'eq' or '!eq'\n\n");
        return;
      }

      //
      // Qword is checked by the following function, no need to double
      // check it above.
      //

      if (!ConvertStringToUInt64(Section, &Value)) {
        ShowMessages("please specify a correct hex value to change the memory "
                     "content\n\n");
        CommandEditMemoryHelp();
        return;
      } else {

        //
        // Add it to the list
        //

        ValuesToEdit.push_back(Value);

        //
        // Keep track of values to modify
        //
        CountOfValues++;

        if (!SetValue) {
          //
          // At least on walue is there
          //
          SetValue = TRUE;
        }
        continue;
      }
    }
  }

  //
  // Fill the structure
  //
  EditMemoryRequest.ProcessId = ProcId;
  EditMemoryRequest.Address = Address;
  EditMemoryRequest.CountOf64Chunks = CountOfValues;

  //
  // Check if address and value are set or not
  //
  if (!SetAddress) {
    ShowMessages("please specify a correct hex address\n\n");
    CommandEditMemoryHelp();
    return;
  }
  if (!SetValue) {
    ShowMessages(
        "please specify a correct hex value as the content to edit\n\n");
    CommandEditMemoryHelp();
    return;
  }
  if (NextIsProcId) {
    ShowMessages("please specify a correct hex value as the process id\n\n");
    CommandEditMemoryHelp();
    return;
  }

  if (!g_DeviceHandle) {
    ShowMessages("Handle not found, probably the driver is not loaded.\n");
    return;
  }

  //
  // Now it's time to put everything together in one structure
  //
  FinalSize = (CountOfValues * sizeof(UINT64)) + SIZEOF_DEBUGGER_EDIT_MEMORY;

  //
  // Set the size
  //
  EditMemoryRequest.FinalStructureSize = FinalSize;

  //
  // Allocate structure + buffer
  //
  FinalBuffer = (UINT64 *)malloc(FinalSize);

  if (!FinalBuffer) {
    ShowMessages("unable to allocate memory\n\n");
    return;
  }

  //
  // Zero the buffer
  //
  ZeroMemory(FinalBuffer, FinalSize);

  //
  // Copy the structure on top of the allocated buffer
  //
  memcpy(FinalBuffer, &EditMemoryRequest, SIZEOF_DEBUGGER_EDIT_MEMORY);

  //
  // Put the values in 64 bit structures
  //
  std::copy(ValuesToEdit.begin(), ValuesToEdit.end(),
            (UINT64 *)((UINT64)FinalBuffer + SIZEOF_DEBUGGER_EDIT_MEMORY));

  Status = DeviceIoControl(g_DeviceHandle,             // Handle to device
                           IOCTL_DEBUGGER_EDIT_MEMORY, // IO Control code
                           FinalBuffer, // Input Buffer to driver.
                           FinalSize,   // Input buffer length
                           NULL,        // Output Buffer from driver.
                           NULL,        // Length of output buffer in bytes.
                           NULL,        // Bytes placed in buffer.
                           NULL         // synchronous call
  );

  if (!Status) {
    ShowMessages("Ioctl failed with code 0x%x\n", GetLastError());
    return;
  }
}