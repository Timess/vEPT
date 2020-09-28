#include "BootMgfw.h"

SHITHOOK BootMgfwShitHook;
EFI_STATUS EFIAPI GetBootMgfwPath(EFI_DEVICE_PATH_PROTOCOL** BootMgfwPathProtocol)
{
	UINTN HandleCount = NULL;
	EFI_STATUS Result;
	EFI_HANDLE* Handles = NULL;
	EFI_DEVICE_PATH* DevicePath = NULL;
	EFI_FILE_HANDLE VolumeHandle;
	EFI_FILE_HANDLE BootMgfwHandle;
	EFI_FILE_IO_INTERFACE* FileSystem = NULL;

	// get all the handles to file systems...
	if (EFI_ERROR((Result = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleCount, &Handles))))
	{
		DBG_PRINT("error getting file system handles -> 0x%p\n", Result);
		return Result;
	}

	// for each handle to the file system, open a protocol with it...
	for (UINT32 Idx = 0u; Idx < HandleCount && !FileSystem; ++Idx)
	{
		if (EFI_ERROR((Result = gBS->OpenProtocol(Handles[Idx], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL))))
		{
			DBG_PRINT("error opening protocol -> 0x%p\n", Result);
			return Result;
		}

		if (EFI_ERROR((Result = FileSystem->OpenVolume(FileSystem, &VolumeHandle))))
		{
			DBG_PRINT("error opening file system -> 0x%p\n", Result);
			return Result;
		}

		// if we found the correct file (\\efi\\microsoft\\boot\\bootmgfw.efi)
		if (!EFI_ERROR(VolumeHandle->Open(VolumeHandle, &BootMgfwHandle, WINDOWS_BOOTMGR_PATH, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY)))
		{
			VolumeHandle->Close(BootMgfwHandle);
			*BootMgfwPathProtocol = FileDevicePath(Handles[Idx], WINDOWS_BOOTMGR_PATH);
			return EFI_SUCCESS;
		}

		if (EFI_ERROR((Result = gBS->CloseProtocol(Handles[Idx], &gEfiSimpleFileSystemProtocolGuid, gImageHandle, NULL))))
		{
			DBG_PRINT("error closing protocol -> 0x%p\n", Result);
			return Result;
		}
	}
	return EFI_ABORTED;
}

EFI_STATUS EFIAPI InstallBootMgfwHooks(EFI_HANDLE ImageHandle)
{
	EFI_STATUS Result = EFI_SUCCESS;
	EFI_LOADED_IMAGE* BootMgfw = NULL;

	if (EFI_ERROR(Result = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&BootMgfw)))
		return Result;

	DBG_PRINT("Module base -> 0x%p\n", BootMgfw->ImageBase);
	DBG_PRINT("Module size -> 0x%x\n", BootMgfw->ImageSize);

	VOID* ArchStartBootApplication = 
		FindPattern(
			BootMgfw->ImageBase,
			BootMgfw->ImageSize,
			START_BOOT_APPLICATION_SIG, 
			START_BOOT_APPLICATION_MASK
		);

	if (!ArchStartBootApplication)
		return EFI_ABORTED;

	DBG_PRINT("ArchStartBootApplication -> 0x%p\n", RESOLVE_RVA(ArchStartBootApplication, 5, 1));
	MakeShitHook(&BootMgfwShitHook, RESOLVE_RVA(ArchStartBootApplication, 5, 1), &ArchStartBootApplicationHook, TRUE);
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ArchStartBootApplicationHook(VOID* AppEntry, VOID* ImageBase, UINT32 ImageSize, UINT8 BootOption, VOID* ReturnArgs)
{
	DisableShitHook(&BootMgfwShitHook);
	VOID* ImgLoadPEImageEx =
		FindPattern(
			ImageBase,
			ImageSize,
			LOAD_PE_IMG_SIG,
			LOAD_PE_IMG_MASK
		);

	Print(L"PE PayLoad Size -> 0x%x\n", PayLoadSize());
	Print(L"winload base -> 0x%p\n", ImageBase);
	Print(L"winload size -> 0x%x\n", ImageSize);
	Print(L"winload.BlImgLoadPEImageEx -> 0x%p\n", RESOLVE_RVA(ImgLoadPEImageEx, 5, 1));

	MakeShitHook(&WinLoadImageShitHook, RESOLVE_RVA(ImgLoadPEImageEx, 5, 1), &BlImgLoadPEImageEx, TRUE);
	return ((IMG_ARCH_START_BOOT_APPLICATION)BootMgfwShitHook.Address)(AppEntry, ImageBase, ImageSize, BootOption, ReturnArgs);
}