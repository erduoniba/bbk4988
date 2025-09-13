// BA4988.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "BA4988.h"

#define MAX_LOADSTRING 100

// 全局变量:
int SCREEN_GAIN = 4;
CHAR szTitle[] = "BA4988模拟器 V1.2.1 - gitee.com/BA4988/BBK-simulator/tree/BA4988"; // 标题栏文本

enum FIXED_WINDOW {
	FIEXD_NONE = 0,
	FIEXD_TOPLEFT,
	FIEXD_TOPRIGHT,
	FIEXD_BOTTOMLEFT,
	FIEXD_BOTTOMRIGHT,
	FIEXD_LEFTCENTER,
	FIEXD_RIGHTCENTER,
	FIEXD_TOPCENTER,
	FIEXD_BOTTOMCENTER,
};
LONG frame_width;
LONG frame_height;
LONG screen_mult = 1;
UINT8 busCycles; // 写Flash Memory阶段
// PRODUCT IDENTIFICATION
UINT8 norFlash[0x35] = { 0 };
UINT8 FlashCommandStage;
UINT32 opCodeToStrTable[0x100];
UINT32 opCodeAddrModeTable[0x100];
HANDLE fileHandles[0x20];
HANDLE fileMappingHandles[0x20];
UINT8* FileMappingList[0x20];
UINT32 FileMappingQuantity; // 映射的文件数量
UINT8 KeyTable[0x100];
RECT LeftIconArea; // 屏幕左侧图标区
RECT rectResources[0x60][2]; // [图标在资源文件上的rect，图标在屏幕(400, 192)上的rect] 
RECT RightIconArea; // 屏幕右侧图标区
UINT8* pVideoMemory; // 显存指针
UINT8 pvBitsLcd[1920];
HBITMAP hbmLcd;
HDC hdcLcd;
HBITMAP hbmScreen;
HDC hdcScreen;
HBITMAP hbmResource;
HDC hdcResource;
HINSTANCE hInst; // 当前实例
HDC hDC;
HWND hWnd;
UINT32 SleepFlag;
UINT32 CycleCounter; // MCU周期计数器
CHAR ProgramDirectory[260]; // 存放程序所在目录
UINT8* pRam;
UINT8* segPageBaseAddrs[0x20];
UINT32 FlashEndAdr;
UINT8* segPageChannelBaseAddrs[0x20][0x10];
UINT32 FlashSize;
UINT8* pFlashMemory;
UINT32 FlashStartAdr;
UINT32 BankSelected;
UINT32 visitAddr;
UINT32 opData; // 立即数
struct CPU
{
	UINT32 A;
	UINT32 S;
	UINT32 X;
	UINT32 Y;
	UINT32 P;
	UINT32 PC;
}cpu;
UINT32 errorAddr;
UINT32 BankAddressList[0x10]; // Bank 映射表
void (*pInstruction[0x100])(); // 指令指针
void (*pAddressingMode[0x100])(); // 指令地址模式指针
UINT32 CyclesNumberTable[0x100]; // 指令周期数表
UINT32 visitFlashAddr;
UINT32 McuOpCode; // opcode
UINT32 Timer1Counter;
UINT32 Timer2Counter;
UINT32 Timer3Counter;
UINT32 Timer4Counter;
UINT32 UniversalTimerCounter;
UINT32 bWantInt; // 中断标志，为1有中断发生
void (*pInterruptHandle[2])();
void (*ReadRegisterFns[0x300])(); // 读寄存器响应函数指针
void (*WriteRegisterFns[0x300])(); // 写寄存器响应函数指针

#ifdef _DEBUG
// 调试用变量
UINT32 stack_list[0x100];
UINT8 stack_list_index = 0x00;
struct _REGEDIT
{
	UINT32 A;
	UINT32 S;
	UINT32 X;
	UINT32 Y;
	UINT32 P;
	UINT32 PC;
	UINT32 opcode;
	UINT8 datas[3];
	UINT32 counter;
} regedits[0x100];
CHAR opCodePrintStrs[60][10];
UINT8 regedits_index = 0;
void InitOpCodePrintStrs();
void debug_save_regedits();
#endif


void CalcWindowSize(RECT* rect, FIXED_WINDOW fixed)
{
	RECT old_rect;
	GetWindowRect(hWnd, &old_rect);
	rect->right = rect->left + 400 * screen_mult;
	rect->bottom = rect->top + 192 * screen_mult;
	AdjustWindowRectEx(rect,
		GetWindowLong(hWnd, GWL_STYLE),
		FALSE,
		GetWindowLong(hWnd, GWL_EXSTYLE)
	);
	LONG width = rect->right - rect->left;
	LONG height = rect->bottom - rect->top;
	if (fixed == FIEXD_LEFTCENTER || fixed == FIEXD_TOPLEFT || fixed == FIEXD_BOTTOMLEFT)
	{
		rect->left = old_rect.left;
		rect->right = old_rect.left + width;
	}
	if (fixed == FIEXD_RIGHTCENTER || fixed == FIEXD_TOPRIGHT || fixed == FIEXD_BOTTOMRIGHT)
	{
		rect->right = old_rect.right;
		rect->left = old_rect.right - width;
	}
	if (fixed == FIEXD_TOPCENTER || fixed == FIEXD_TOPLEFT || fixed == FIEXD_TOPRIGHT)
	{
		rect->top = old_rect.top;
		rect->bottom = old_rect.top + height;
	}
	if (fixed == FIEXD_BOTTOMCENTER || fixed == FIEXD_BOTTOMLEFT || fixed == FIEXD_BOTTOMRIGHT)
	{
		rect->bottom = old_rect.bottom;
		rect->top = old_rect.bottom - height;
	}
	if (fixed == FIEXD_LEFTCENTER || fixed == FIEXD_RIGHTCENTER)
	{
		rect->top = __max(0, (old_rect.bottom + old_rect.top - height) / 2);
		rect->bottom = rect->top + height;
	}
	if (fixed == FIEXD_TOPCENTER || fixed == FIEXD_BOTTOMCENTER)
	{
		rect->left = (old_rect.right + old_rect.left - width) / 2;
		rect->right = rect->left + width;
	}
}

// done
void FlashInit()
{
	FlashCommandStage = 0x00;
	busCycles = 0x01;

	// CFI QUERY IDENTIFICATION STRING
	norFlash[0x10] = 0x51;
	norFlash[0x11] = 0x52;
	norFlash[0x12] = 0x59;
	norFlash[0x13] = 0x01;
	norFlash[0x14] = 0x07;
	norFlash[0x15] = 0x00;
	norFlash[0x16] = 0x00;
	norFlash[0x17] = 0x00;
	norFlash[0x18] = 0x00;
	norFlash[0x19] = 0x00;
	norFlash[0x1A] = 0x00;
	// SYSTEM INTERFACE INFORMATION
	norFlash[0x1B] = 0x27;
	norFlash[0x1C] = 0x36;
	norFlash[0x1D] = 0x00;
	norFlash[0x1E] = 0x00;
	norFlash[0x1F] = 0x04;
	norFlash[0x20] = 0x00;
	norFlash[0x21] = 0x04;
	norFlash[0x22] = 0x06;
	norFlash[0x23] = 0x01;
	norFlash[0x24] = 0x00;
	norFlash[0x25] = 0x01;
	norFlash[0x26] = 0x01;
	// DEVICE GEOMETRY INFORMATION
	norFlash[0x27] = 0x15; // 1<<0x15 = 2 MB
	norFlash[0x28] = 0x00;
	norFlash[0x29] = 0x00;
	norFlash[0x2A] = 0x00;
	norFlash[0x2B] = 0x00;
	norFlash[0x2C] = 0x02;
	norFlash[0x2D] = 0xFF;
	norFlash[0x2E] = 0x01; // 0x01FF+1 = 512 sectors
	norFlash[0x2F] = 0x10;
	norFlash[0x30] = 0x00; // 0x0010*256 = 4 KB/sector
	norFlash[0x31] = 0x1F;
	norFlash[0x32] = 0x00; // 0x001F+1 = 32 blocks
	norFlash[0x33] = 0x00;
	norFlash[0x34] = 0x01; // 0x0100*256 = 64KB/block
}

// 从Flash读一个字节
UINT8 ReadFlash(UINT32 address)
{
	if (FlashCommandStage == 0 || FlashCommandStage == 1)
	{
		return pFlashMemory[address];
	}
	else
	{
		return norFlash[address];
	}
}

// todo
void WriteFlash(UINT32 address, UINT8 data)
{
	switch (busCycles - 1)
	{
	case 0://1180
		if (address == 0x5555 && data == 0xAA)
		{
			busCycles++;
		}
		else if (data == 0xF0)
		{
			// Software ID Exit/CFI Exit
			FlashCommandStage = 0;
		}
		break;
	case 1://11C4
	case 4://11C4
		if (address == 0x2AAA && data == 0x55)
		{
			busCycles++;
		}
		break;
	case 2://11EE
		if (address != 0x5555)
		{
			return;
		}
		switch (data - 0x90)
		{
		case 0x10://122B Word-Program
			FlashCommandStage = 1;
			break;
		case 0x00://1234 Software ID Entry
			FlashCommandStage = 2;
			busCycles = 0;
			break;
		case 0x08://1244 CFI Query Entry
			FlashCommandStage = 3;
			busCycles = 0;
			break;
		case 0x60://1254 Software ID Exit/CFI Exit
			FlashCommandStage = 0;
			busCycles = 0;
			break;
		}
		busCycles += 1;
		break;
	case 3://1273
		if (FlashCommandStage == 1)
		{ // Word-Program
			pFlashMemory[address] = data;
			FlashCommandStage = 0;
			busCycles = 1;
		}
		else if ((address == 0x5555) && (data == 0xAA))
		{
			busCycles += 1;
		}
		break;
	case 5://12CC
		switch (data)
		{
		case 0x10: // Chip-Erase
			memset(pFlashMemory, 0xFF, 0x200000);  // Erase 2MB
			break;
		case 0x30: // Sector-Erase
			memset(pFlashMemory + (address & 0x1FF000), 0xFF, 0x1000); // Erase 4KB
			break;
		case 0x50: // Block-Erase
			memset(pFlashMemory + (address & 0x1F0000), 0xFF, 0x10000); // Erase 64KB
			break;
		}
		FlashCommandStage = 0;
		busCycles = 1;
		break;
	}
}

// done 仅在debug用
void InitOpCodeMapping()
{
	opCodeToStrTable[0x00] = 0x0B; // 操作码不同，但助记符可能相同
	opCodeAddrModeTable[0x00] = 0x00;
	opCodeToStrTable[0x01] = 0x24;
	opCodeAddrModeTable[0x01] = 0x0A;
	opCodeToStrTable[0x02] = 0x0B;
	opCodeAddrModeTable[0x02] = 0x00;
	opCodeToStrTable[0x03] = 0x0B;
	opCodeAddrModeTable[0x03] = 0x00;
	opCodeToStrTable[0x04] = 0x0B;
	opCodeAddrModeTable[0x04] = 0x00;
	opCodeToStrTable[0x05] = 0x24;
	opCodeAddrModeTable[0x05] = 0x07;
	opCodeToStrTable[0x06] = 0x02;
	opCodeAddrModeTable[0x06] = 0x07;
	opCodeToStrTable[0x07] = 0x0B;
	opCodeAddrModeTable[0x07] = 0x00;
	opCodeToStrTable[0x08] = 0x26;
	opCodeAddrModeTable[0x08] = 0x00;
	opCodeToStrTable[0x09] = 0x24;
	opCodeAddrModeTable[0x09] = 0x01;
	opCodeToStrTable[0x0A] = 0x03;
	opCodeAddrModeTable[0x0A] = 0x00;
	opCodeToStrTable[0x0B] = 0x0B;
	opCodeAddrModeTable[0x0B] = 0x00;
	opCodeToStrTable[0x0C] = 0x0B;
	opCodeAddrModeTable[0x0C] = 0x00;
	opCodeToStrTable[0x0D] = 0x24;
	opCodeAddrModeTable[0x0D] = 0x02;
	opCodeToStrTable[0x0E] = 0x02;
	opCodeAddrModeTable[0x0E] = 0x02;
	opCodeToStrTable[0x0F] = 0x0B;
	opCodeAddrModeTable[0x0F] = 0x00;
	opCodeToStrTable[0x10] = 0x0A;
	opCodeAddrModeTable[0x10] = 0x03;
	opCodeToStrTable[0x11] = 0x24;
	opCodeAddrModeTable[0x11] = 0x0B;
	opCodeToStrTable[0x12] = 0x0B;
	opCodeAddrModeTable[0x12] = 0x00;
	opCodeToStrTable[0x13] = 0x0B;
	opCodeAddrModeTable[0x13] = 0x00;
	opCodeToStrTable[0x14] = 0x0B;
	opCodeAddrModeTable[0x14] = 0x00;
	opCodeToStrTable[0x15] = 0x24;
	opCodeAddrModeTable[0x15] = 0x08;
	opCodeToStrTable[0x16] = 0x02;
	opCodeAddrModeTable[0x16] = 0x08;
	opCodeToStrTable[0x17] = 0x0B;
	opCodeAddrModeTable[0x17] = 0x00;
	opCodeToStrTable[0x18] = 0x0E;
	opCodeAddrModeTable[0x18] = 0x00;
	opCodeToStrTable[0x19] = 0x24;
	opCodeAddrModeTable[0x19] = 0x06;
	opCodeToStrTable[0x1A] = 0x0B;
	opCodeAddrModeTable[0x1A] = 0x00;
	opCodeToStrTable[0x1B] = 0x0B;
	opCodeAddrModeTable[0x1B] = 0x00;
	opCodeToStrTable[0x1C] = 0x0B;
	opCodeAddrModeTable[0x1C] = 0x00;
	opCodeToStrTable[0x1D] = 0x24;
	opCodeAddrModeTable[0x1D] = 0x05;
	opCodeToStrTable[0x1E] = 0x02;
	opCodeAddrModeTable[0x1E] = 0x05;
	opCodeToStrTable[0x1F] = 0x0B;
	opCodeAddrModeTable[0x1F] = 0x00;
	opCodeToStrTable[0x20] = 0x1D;
	opCodeAddrModeTable[0x20] = 0x02;
	opCodeToStrTable[0x21] = 0x01;
	opCodeAddrModeTable[0x21] = 0x0A;
	opCodeToStrTable[0x22] = 0x0B;
	opCodeAddrModeTable[0x22] = 0x00;
	opCodeToStrTable[0x23] = 0x0B;
	opCodeAddrModeTable[0x23] = 0x00;
	opCodeToStrTable[0x24] = 0x07;
	opCodeAddrModeTable[0x24] = 0x07;
	opCodeToStrTable[0x25] = 0x01;
	opCodeAddrModeTable[0x25] = 0x07;
	opCodeToStrTable[0x26] = 0x29;
	opCodeAddrModeTable[0x26] = 0x07;
	opCodeToStrTable[0x27] = 0x0B;
	opCodeAddrModeTable[0x27] = 0x00;
	opCodeToStrTable[0x28] = 0x28;
	opCodeAddrModeTable[0x28] = 0x00;
	opCodeToStrTable[0x29] = 0x01;
	opCodeAddrModeTable[0x29] = 0x01;
	opCodeToStrTable[0x2A] = 0x2A;
	opCodeAddrModeTable[0x2A] = 0x00;
	opCodeToStrTable[0x2B] = 0x0B;
	opCodeAddrModeTable[0x2B] = 0x00;
	opCodeToStrTable[0x2C] = 0x07;
	opCodeAddrModeTable[0x2C] = 0x02;
	opCodeToStrTable[0x2D] = 0x01;
	opCodeAddrModeTable[0x2D] = 0x02;
	opCodeToStrTable[0x2E] = 0x29;
	opCodeAddrModeTable[0x2E] = 0x02;
	opCodeToStrTable[0x2F] = 0x0B;
	opCodeAddrModeTable[0x2F] = 0x00;
	opCodeToStrTable[0x30] = 0x08;
	opCodeAddrModeTable[0x30] = 0x03;
	opCodeToStrTable[0x31] = 0x01;
	opCodeAddrModeTable[0x31] = 0x0B;
	opCodeToStrTable[0x32] = 0x0B;
	opCodeAddrModeTable[0x32] = 0x00;
	opCodeToStrTable[0x33] = 0x0B;
	opCodeAddrModeTable[0x33] = 0x00;
	opCodeToStrTable[0x34] = 0x07;
	opCodeAddrModeTable[0x34] = 0x08;
	opCodeToStrTable[0x35] = 0x01;
	opCodeAddrModeTable[0x35] = 0x08;
	opCodeToStrTable[0x36] = 0x29;
	opCodeAddrModeTable[0x36] = 0x08;
	opCodeToStrTable[0x37] = 0x0B;
	opCodeAddrModeTable[0x37] = 0x00;
	opCodeToStrTable[0x38] = 0x30;
	opCodeAddrModeTable[0x38] = 0x00;
	opCodeToStrTable[0x39] = 0x01;
	opCodeAddrModeTable[0x39] = 0x06;
	opCodeToStrTable[0x3A] = 0x0B;
	opCodeAddrModeTable[0x3A] = 0x00;
	opCodeToStrTable[0x3B] = 0x0B;
	opCodeAddrModeTable[0x3B] = 0x00;
	opCodeToStrTable[0x3C] = 0x07;
	opCodeAddrModeTable[0x3C] = 0x05;
	opCodeToStrTable[0x3D] = 0x01;
	opCodeAddrModeTable[0x3D] = 0x05;
	opCodeToStrTable[0x3E] = 0x29;
	opCodeAddrModeTable[0x3E] = 0x05;
	opCodeToStrTable[0x3F] = 0x0B;
	opCodeAddrModeTable[0x3F] = 0x00;
	opCodeToStrTable[0x40] = 0x2D;
	opCodeAddrModeTable[0x40] = 0x00;
	opCodeToStrTable[0x41] = 0x18;
	opCodeAddrModeTable[0x41] = 0x0A;
	opCodeToStrTable[0x42] = 0x0B;
	opCodeAddrModeTable[0x42] = 0x00;
	opCodeToStrTable[0x43] = 0x0B;
	opCodeAddrModeTable[0x43] = 0x00;
	opCodeToStrTable[0x44] = 0x0B;
	opCodeAddrModeTable[0x44] = 0x00;
	opCodeToStrTable[0x45] = 0x18;
	opCodeAddrModeTable[0x45] = 0x07;
	opCodeToStrTable[0x46] = 0x21;
	opCodeAddrModeTable[0x46] = 0x07;
	opCodeToStrTable[0x47] = 0x0B;
	opCodeAddrModeTable[0x47] = 0x00;
	opCodeToStrTable[0x48] = 0x25;
	opCodeAddrModeTable[0x48] = 0x00;
	opCodeToStrTable[0x49] = 0x18;
	opCodeAddrModeTable[0x49] = 0x01;
	opCodeToStrTable[0x4A] = 0x22;
	opCodeAddrModeTable[0x4A] = 0x00;
	opCodeToStrTable[0x4B] = 0x0B;
	opCodeAddrModeTable[0x4B] = 0x00;
	opCodeToStrTable[0x4C] = 0x1C;
	opCodeAddrModeTable[0x4C] = 0x02;
	opCodeToStrTable[0x4D] = 0x18;
	opCodeAddrModeTable[0x4D] = 0x02;
	opCodeToStrTable[0x4E] = 0x21;
	opCodeAddrModeTable[0x4E] = 0x02;
	opCodeToStrTable[0x4F] = 0x0B;
	opCodeAddrModeTable[0x4F] = 0x00;
	opCodeToStrTable[0x50] = 0x0C;
	opCodeAddrModeTable[0x50] = 0x03;
	opCodeToStrTable[0x51] = 0x18;
	opCodeAddrModeTable[0x51] = 0x0B;
	opCodeToStrTable[0x52] = 0x0B;
	opCodeAddrModeTable[0x52] = 0x00;
	opCodeToStrTable[0x53] = 0x0B;
	opCodeAddrModeTable[0x53] = 0x00;
	opCodeToStrTable[0x54] = 0x0B;
	opCodeAddrModeTable[0x54] = 0x00;
	opCodeToStrTable[0x55] = 0x18;
	opCodeAddrModeTable[0x55] = 0x08;
	opCodeToStrTable[0x56] = 0x21;
	opCodeAddrModeTable[0x56] = 0x08;
	opCodeToStrTable[0x57] = 0x0B;
	opCodeAddrModeTable[0x57] = 0x00;
	opCodeToStrTable[0x58] = 0x10;
	opCodeAddrModeTable[0x58] = 0x00;
	opCodeToStrTable[0x59] = 0x18;
	opCodeAddrModeTable[0x59] = 0x06;
	opCodeToStrTable[0x5A] = 0x0B;
	opCodeAddrModeTable[0x5A] = 0x00;
	opCodeToStrTable[0x5B] = 0x0B;
	opCodeAddrModeTable[0x5B] = 0x00;
	opCodeToStrTable[0x5C] = 0x0B;
	opCodeAddrModeTable[0x5C] = 0x00;
	opCodeToStrTable[0x5D] = 0x18;
	opCodeAddrModeTable[0x5D] = 0x05;
	opCodeToStrTable[0x5E] = 0x21;
	opCodeAddrModeTable[0x5E] = 0x05;
	opCodeToStrTable[0x5F] = 0x0B;
	opCodeAddrModeTable[0x5F] = 0x00;
	opCodeToStrTable[0x60] = 0x2E;
	opCodeAddrModeTable[0x60] = 0x00;
	opCodeToStrTable[0x61] = 0x00;
	opCodeAddrModeTable[0x61] = 0x0A;
	opCodeToStrTable[0x62] = 0x0B;
	opCodeAddrModeTable[0x62] = 0x00;
	opCodeToStrTable[0x63] = 0x0B;
	opCodeAddrModeTable[0x63] = 0x00;
	opCodeToStrTable[0x64] = 0x0B;
	opCodeAddrModeTable[0x64] = 0x00;
	opCodeToStrTable[0x65] = 0x00;
	opCodeAddrModeTable[0x65] = 0x07;
	opCodeToStrTable[0x66] = 0x2B;
	opCodeAddrModeTable[0x66] = 0x07;
	opCodeToStrTable[0x67] = 0x0B;
	opCodeAddrModeTable[0x67] = 0x00;
	opCodeToStrTable[0x68] = 0x27;
	opCodeAddrModeTable[0x68] = 0x00;
	opCodeToStrTable[0x69] = 0x00;
	opCodeAddrModeTable[0x69] = 0x01;
	opCodeToStrTable[0x6A] = 0x2C;
	opCodeAddrModeTable[0x6A] = 0x00;
	opCodeToStrTable[0x6B] = 0x0B;
	opCodeAddrModeTable[0x6B] = 0x00;
	opCodeToStrTable[0x6C] = 0x1C;
	opCodeAddrModeTable[0x6C] = 0x04;
	opCodeToStrTable[0x6D] = 0x00;
	opCodeAddrModeTable[0x6D] = 0x02;
	opCodeToStrTable[0x6E] = 0x2B;
	opCodeAddrModeTable[0x6E] = 0x02;
	opCodeToStrTable[0x6F] = 0x0B;
	opCodeAddrModeTable[0x6F] = 0x00;
	opCodeToStrTable[0x70] = 0x0D;
	opCodeAddrModeTable[0x70] = 0x03;
	opCodeToStrTable[0x71] = 0x00;
	opCodeAddrModeTable[0x71] = 0x0B;
	opCodeToStrTable[0x72] = 0x0B;
	opCodeAddrModeTable[0x72] = 0x00;
	opCodeToStrTable[0x73] = 0x0B;
	opCodeAddrModeTable[0x73] = 0x00;
	opCodeToStrTable[0x74] = 0x0B;
	opCodeAddrModeTable[0x74] = 0x00;
	opCodeToStrTable[0x75] = 0x00;
	opCodeAddrModeTable[0x75] = 0x08;
	opCodeToStrTable[0x76] = 0x2B;
	opCodeAddrModeTable[0x76] = 0x08;
	opCodeToStrTable[0x77] = 0x0B;
	opCodeAddrModeTable[0x77] = 0x00;
	opCodeToStrTable[0x78] = 0x32;
	opCodeAddrModeTable[0x78] = 0x00;
	opCodeToStrTable[0x79] = 0x00;
	opCodeAddrModeTable[0x79] = 0x06;
	opCodeToStrTable[0x7A] = 0x0B;
	opCodeAddrModeTable[0x7A] = 0x00;
	opCodeToStrTable[0x7B] = 0x0B;
	opCodeAddrModeTable[0x7B] = 0x00;
	opCodeToStrTable[0x7C] = 0x0B;
	opCodeAddrModeTable[0x7C] = 0x00;
	opCodeToStrTable[0x7D] = 0x00;
	opCodeAddrModeTable[0x7D] = 0x05;
	opCodeToStrTable[0x7E] = 0x2B;
	opCodeAddrModeTable[0x7E] = 0x05;
	opCodeToStrTable[0x7F] = 0x0B;
	opCodeAddrModeTable[0x7F] = 0x00;
	opCodeToStrTable[0x80] = 0x0B;
	opCodeAddrModeTable[0x80] = 0x03;
	opCodeToStrTable[0x81] = 0x33;
	opCodeAddrModeTable[0x81] = 0x0A;
	opCodeToStrTable[0x82] = 0x0B;
	opCodeAddrModeTable[0x82] = 0x00;
	opCodeToStrTable[0x83] = 0x0B;
	opCodeAddrModeTable[0x83] = 0x00;
	opCodeToStrTable[0x84] = 0x35;
	opCodeAddrModeTable[0x84] = 0x07;
	opCodeToStrTable[0x85] = 0x33;
	opCodeAddrModeTable[0x85] = 0x07;
	opCodeToStrTable[0x86] = 0x34;
	opCodeAddrModeTable[0x86] = 0x07;
	opCodeToStrTable[0x87] = 0x0B;
	opCodeAddrModeTable[0x87] = 0x00;
	opCodeToStrTable[0x88] = 0x17;
	opCodeAddrModeTable[0x88] = 0x00;
	opCodeToStrTable[0x89] = 0x07;
	opCodeAddrModeTable[0x89] = 0x01;
	opCodeToStrTable[0x8A] = 0x39;
	opCodeAddrModeTable[0x8A] = 0x00;
	opCodeToStrTable[0x8B] = 0x0B;
	opCodeAddrModeTable[0x8B] = 0x00;
	opCodeToStrTable[0x8C] = 0x35;
	opCodeAddrModeTable[0x8C] = 0x02;
	opCodeToStrTable[0x8D] = 0x33;
	opCodeAddrModeTable[0x8D] = 0x02;
	opCodeToStrTable[0x8E] = 0x34;
	opCodeAddrModeTable[0x8E] = 0x02;
	opCodeToStrTable[0x8F] = 0x0B;
	opCodeAddrModeTable[0x8F] = 0x00;
	opCodeToStrTable[0x90] = 0x04;
	opCodeAddrModeTable[0x90] = 0x03;
	opCodeToStrTable[0x91] = 0x33;
	opCodeAddrModeTable[0x91] = 0x0B;
	opCodeToStrTable[0x92] = 0x0B;
	opCodeAddrModeTable[0x92] = 0x00;
	opCodeToStrTable[0x93] = 0x0B;
	opCodeAddrModeTable[0x93] = 0x00;
	opCodeToStrTable[0x94] = 0x35;
	opCodeAddrModeTable[0x94] = 0x08;
	opCodeToStrTable[0x95] = 0x33;
	opCodeAddrModeTable[0x95] = 0x08;
	opCodeToStrTable[0x96] = 0x34;
	opCodeAddrModeTable[0x96] = 0x09;
	opCodeToStrTable[0x97] = 0x0B;
	opCodeAddrModeTable[0x97] = 0x00;
	opCodeToStrTable[0x98] = 0x3B;
	opCodeAddrModeTable[0x98] = 0x00;
	opCodeToStrTable[0x99] = 0x33;
	opCodeAddrModeTable[0x99] = 0x06;
	opCodeToStrTable[0x9A] = 0x3A;
	opCodeAddrModeTable[0x9A] = 0x00;
	opCodeToStrTable[0x9B] = 0x0B;
	opCodeAddrModeTable[0x9B] = 0x00;
	opCodeToStrTable[0x9C] = 0x0B;
	opCodeAddrModeTable[0x9C] = 0x00;
	opCodeToStrTable[0x9D] = 0x33;
	opCodeAddrModeTable[0x9D] = 0x05;
	opCodeToStrTable[0x9E] = 0x0B;
	opCodeAddrModeTable[0x9E] = 0x00;
	opCodeToStrTable[0x9F] = 0x0B;
	opCodeAddrModeTable[0x9F] = 0x00;
	opCodeToStrTable[0xA0] = 0x20;
	opCodeAddrModeTable[0xA0] = 0x01;
	opCodeToStrTable[0xA1] = 0x1E;
	opCodeAddrModeTable[0xA1] = 0x0A;
	opCodeToStrTable[0xA2] = 0x1F;
	opCodeAddrModeTable[0xA2] = 0x01;
	opCodeToStrTable[0xA3] = 0x0B;
	opCodeAddrModeTable[0xA3] = 0x00;
	opCodeToStrTable[0xA4] = 0x20;
	opCodeAddrModeTable[0xA4] = 0x07;
	opCodeToStrTable[0xA5] = 0x1E;
	opCodeAddrModeTable[0xA5] = 0x07;
	opCodeToStrTable[0xA6] = 0x1F;
	opCodeAddrModeTable[0xA6] = 0x07;
	opCodeToStrTable[0xA7] = 0x0B;
	opCodeAddrModeTable[0xA7] = 0x00;
	opCodeToStrTable[0xA8] = 0x37;
	opCodeAddrModeTable[0xA8] = 0x00;
	opCodeToStrTable[0xA9] = 0x1E;
	opCodeAddrModeTable[0xA9] = 0x01;
	opCodeToStrTable[0xAA] = 0x36;
	opCodeAddrModeTable[0xAA] = 0x00;
	opCodeToStrTable[0xAB] = 0x0B;
	opCodeAddrModeTable[0xAB] = 0x00;
	opCodeToStrTable[0xAC] = 0x20;
	opCodeAddrModeTable[0xAC] = 0x02;
	opCodeToStrTable[0xAD] = 0x1E;
	opCodeAddrModeTable[0xAD] = 0x02;
	opCodeToStrTable[0xAE] = 0x1F;
	opCodeAddrModeTable[0xAE] = 0x02;
	opCodeToStrTable[0xAF] = 0x0B;
	opCodeAddrModeTable[0xAF] = 0x00;
	opCodeToStrTable[0xB0] = 0x05;
	opCodeAddrModeTable[0xB0] = 0x03;
	opCodeToStrTable[0xB1] = 0x1E;
	opCodeAddrModeTable[0xB1] = 0x0B;
	opCodeToStrTable[0xB2] = 0x0B;
	opCodeAddrModeTable[0xB2] = 0x00;
	opCodeToStrTable[0xB3] = 0x0B;
	opCodeAddrModeTable[0xB3] = 0x00;
	opCodeToStrTable[0xB4] = 0x20;
	opCodeAddrModeTable[0xB4] = 0x08;
	opCodeToStrTable[0xB5] = 0x1E;
	opCodeAddrModeTable[0xB5] = 0x08;
	opCodeToStrTable[0xB6] = 0x1F;
	opCodeAddrModeTable[0xB6] = 0x09;
	opCodeToStrTable[0xB7] = 0x0B;
	opCodeAddrModeTable[0xB7] = 0x00;
	opCodeToStrTable[0xB8] = 0x11;
	opCodeAddrModeTable[0xB8] = 0x00;
	opCodeToStrTable[0xB9] = 0x1E;
	opCodeAddrModeTable[0xB9] = 0x06;
	opCodeToStrTable[0xBA] = 0x38;
	opCodeAddrModeTable[0xBA] = 0x00;
	opCodeToStrTable[0xBB] = 0x0B;
	opCodeAddrModeTable[0xBB] = 0x00;
	opCodeToStrTable[0xBC] = 0x20;
	opCodeAddrModeTable[0xBC] = 0x05;
	opCodeToStrTable[0xBD] = 0x1E;
	opCodeAddrModeTable[0xBD] = 0x05;
	opCodeToStrTable[0xBE] = 0x1F;
	opCodeAddrModeTable[0xBE] = 0x06;
	opCodeToStrTable[0xBF] = 0x0B;
	opCodeAddrModeTable[0xBF] = 0x00;
	opCodeToStrTable[0xC0] = 0x14;
	opCodeAddrModeTable[0xC0] = 0x01;
	opCodeToStrTable[0xC1] = 0x12;
	opCodeAddrModeTable[0xC1] = 0x0A;
	opCodeToStrTable[0xC2] = 0x0B;
	opCodeAddrModeTable[0xC2] = 0x00;
	opCodeToStrTable[0xC3] = 0x0B;
	opCodeAddrModeTable[0xC3] = 0x00;
	opCodeToStrTable[0xC4] = 0x14;
	opCodeAddrModeTable[0xC4] = 0x07;
	opCodeToStrTable[0xC5] = 0x12;
	opCodeAddrModeTable[0xC5] = 0x07;
	opCodeToStrTable[0xC6] = 0x15;
	opCodeAddrModeTable[0xC6] = 0x07;
	opCodeToStrTable[0xC7] = 0x0B;
	opCodeAddrModeTable[0xC7] = 0x00;
	opCodeToStrTable[0xC8] = 0x1B;
	opCodeAddrModeTable[0xC8] = 0x00;
	opCodeToStrTable[0xC9] = 0x12;
	opCodeAddrModeTable[0xC9] = 0x01;
	opCodeToStrTable[0xCA] = 0x16;
	opCodeAddrModeTable[0xCA] = 0x00;
	opCodeToStrTable[0xCB] = 0x0B;
	opCodeAddrModeTable[0xCB] = 0x00;
	opCodeToStrTable[0xCC] = 0x14;
	opCodeAddrModeTable[0xCC] = 0x02;
	opCodeToStrTable[0xCD] = 0x12;
	opCodeAddrModeTable[0xCD] = 0x02;
	opCodeToStrTable[0xCE] = 0x15;
	opCodeAddrModeTable[0xCE] = 0x02;
	opCodeToStrTable[0xCF] = 0x0B;
	opCodeAddrModeTable[0xCF] = 0x00;
	opCodeToStrTable[0xD0] = 0x09;
	opCodeAddrModeTable[0xD0] = 0x03;
	opCodeToStrTable[0xD1] = 0x12;
	opCodeAddrModeTable[0xD1] = 0x0B;
	opCodeToStrTable[0xD2] = 0x0B;
	opCodeAddrModeTable[0xD2] = 0x00;
	opCodeToStrTable[0xD3] = 0x0B;
	opCodeAddrModeTable[0xD3] = 0x00;
	opCodeToStrTable[0xD4] = 0x0B;
	opCodeAddrModeTable[0xD4] = 0x00;
	opCodeToStrTable[0xD5] = 0x12;
	opCodeAddrModeTable[0xD5] = 0x08;
	opCodeToStrTable[0xD6] = 0x15;
	opCodeAddrModeTable[0xD6] = 0x08;
	opCodeToStrTable[0xD7] = 0x0B;
	opCodeAddrModeTable[0xD7] = 0x00;
	opCodeToStrTable[0xD8] = 0x0F;
	opCodeAddrModeTable[0xD8] = 0x00;
	opCodeToStrTable[0xD9] = 0x12;
	opCodeAddrModeTable[0xD9] = 0x06;
	opCodeToStrTable[0xDA] = 0x0B;
	opCodeAddrModeTable[0xDA] = 0x00;
	opCodeToStrTable[0xDB] = 0x0B;
	opCodeAddrModeTable[0xDB] = 0x00;
	opCodeToStrTable[0xDC] = 0x0B;
	opCodeAddrModeTable[0xDC] = 0x00;
	opCodeToStrTable[0xDD] = 0x12;
	opCodeAddrModeTable[0xDD] = 0x05;
	opCodeToStrTable[0xDE] = 0x15;
	opCodeAddrModeTable[0xDE] = 0x05;
	opCodeToStrTable[0xDF] = 0x0B;
	opCodeAddrModeTable[0xDF] = 0x00;
	opCodeToStrTable[0xE0] = 0x13;
	opCodeAddrModeTable[0xE0] = 0x01;
	opCodeToStrTable[0xE1] = 0x2F;
	opCodeAddrModeTable[0xE1] = 0x0A;
	opCodeToStrTable[0xE2] = 0x0B;
	opCodeAddrModeTable[0xE2] = 0x00;
	opCodeToStrTable[0xE3] = 0x0B;
	opCodeAddrModeTable[0xE3] = 0x00;
	opCodeToStrTable[0xE4] = 0x13;
	opCodeAddrModeTable[0xE4] = 0x07;
	opCodeToStrTable[0xE5] = 0x2F;
	opCodeAddrModeTable[0xE5] = 0x07;
	opCodeToStrTable[0xE6] = 0x19;
	opCodeAddrModeTable[0xE6] = 0x07;
	opCodeToStrTable[0xE7] = 0x0B;
	opCodeAddrModeTable[0xE7] = 0x00;
	opCodeToStrTable[0xE8] = 0x1A;
	opCodeAddrModeTable[0xE8] = 0x00;
	opCodeToStrTable[0xE9] = 0x2F;
	opCodeAddrModeTable[0xE9] = 0x01;
	opCodeToStrTable[0xEA] = 0x23;
	opCodeAddrModeTable[0xEA] = 0x00;
	opCodeToStrTable[0xEB] = 0x0B;
	opCodeAddrModeTable[0xEB] = 0x00;
	opCodeToStrTable[0xEC] = 0x13;
	opCodeAddrModeTable[0xEC] = 0x02;
	opCodeToStrTable[0xED] = 0x2F;
	opCodeAddrModeTable[0xED] = 0x02;
	opCodeToStrTable[0xEE] = 0x19;
	opCodeAddrModeTable[0xEE] = 0x02;
	opCodeToStrTable[0xEF] = 0x0B;
	opCodeAddrModeTable[0xEF] = 0x00;
	opCodeToStrTable[0xF0] = 0x06;
	opCodeAddrModeTable[0xF0] = 0x03;
	opCodeToStrTable[0xF1] = 0x2F;
	opCodeAddrModeTable[0xF1] = 0x0B;
	opCodeToStrTable[0xF2] = 0x0B;
	opCodeAddrModeTable[0xF2] = 0x00;
	opCodeToStrTable[0xF3] = 0x0B;
	opCodeAddrModeTable[0xF3] = 0x00;
	opCodeToStrTable[0xF4] = 0x0B;
	opCodeAddrModeTable[0xF4] = 0x00;
	opCodeToStrTable[0xF5] = 0x2F;
	opCodeAddrModeTable[0xF5] = 0x08;
	opCodeToStrTable[0xF6] = 0x19;
	opCodeAddrModeTable[0xF6] = 0x08;
	opCodeToStrTable[0xF7] = 0x0B;
	opCodeAddrModeTable[0xF7] = 0x00;
	opCodeToStrTable[0xF8] = 0x31;
	opCodeAddrModeTable[0xF8] = 0x00;
	opCodeToStrTable[0xF9] = 0x2F;
	opCodeAddrModeTable[0xF9] = 0x06;
	opCodeToStrTable[0xFA] = 0x0B;
	opCodeAddrModeTable[0xFA] = 0x00;
	opCodeToStrTable[0xFB] = 0x0B;
	opCodeAddrModeTable[0xFB] = 0x00;
	opCodeToStrTable[0xFC] = 0x0B;
	opCodeAddrModeTable[0xFC] = 0x00;
	opCodeToStrTable[0xFD] = 0x2F;
	opCodeAddrModeTable[0xFD] = 0x05;
	opCodeToStrTable[0xFE] = 0x19;
	opCodeAddrModeTable[0xFE] = 0x05;
	opCodeToStrTable[0xFF] = 0x0B;
	opCodeAddrModeTable[0xFF] = 0x00;
}

// done
BOOL AllocFileMemory()
{
	memset(fileHandles, 0, sizeof(fileHandles));
	memset(fileMappingHandles, 0, sizeof(fileMappingHandles));
	memset(FileMappingList, 0, sizeof(FileMappingList));
	FileMappingQuantity = 0;
	return TRUE;
}

// done
UINT8* DoFileMapping(LPCSTR lpFileName, UINT32 writable)
{
	UINT32 mindex = FileMappingQuantity;
	UINT32 dwDesiredAccess = GENERIC_READ;
	if (writable)
	{
		dwDesiredAccess |= GENERIC_WRITE;
	}
	fileHandles[mindex] = CreateFile(lpFileName, dwDesiredAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fileHandles[mindex] == INVALID_HANDLE_VALUE)
	{
		MessageBox(NULL, lpFileName, "打开文件失败", MB_OK);
		return NULL;
	}
	UINT32 flProtect = writable ? PAGE_READWRITE : PAGE_READONLY;
	fileMappingHandles[mindex] = CreateFileMapping(fileHandles[mindex], NULL, flProtect, 0, 0, NULL);
	if (fileMappingHandles[mindex] != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(fileMappingHandles[mindex]);
		fileMappingHandles[mindex] = NULL;
		MessageBox(NULL, lpFileName, "文件已经被映射", MB_OK);
	}
	if (fileMappingHandles[mindex] == NULL)
	{
		CloseHandle(fileHandles[mindex]);
		fileHandles[mindex] = NULL;
		MessageBox(NULL, lpFileName, "创建映射失败", MB_OK);
		return NULL;
	}
	UINT32 accessMode = writable ? FILE_MAP_WRITE : FILE_MAP_READ;
	FileMappingList[mindex] = (UINT8*)MapViewOfFile(fileMappingHandles[mindex], accessMode, 0, 0, 0);
	if (FileMappingList[mindex] == NULL)
	{
		MessageBox(NULL, lpFileName, "映射文件失败", MB_OK);
		CloseHandle(fileMappingHandles[mindex]);
		CloseHandle(fileHandles[mindex]);
		fileMappingHandles[mindex] = NULL;
		fileHandles[mindex] = NULL;
		return NULL;
	}
	FileMappingQuantity += 1;
	return FileMappingList[mindex];
}

// done
BOOL ReleaseFileMappings()
{
	for (UINT32 i = 0; i < FileMappingQuantity; i++)
	{
		UnmapViewOfFile(FileMappingList[i]);
		CloseHandle(fileMappingHandles[i]);
		CloseHandle(fileHandles[i]);
	}
	FileMappingQuantity = 0;
	return TRUE;
}

// 按键响应
void KeyResponse(UINT32 key)
{
	if (KeyTable[key] == 0xFF) {
		return;
	}
	SleepFlag = 0;
	pRam[0x200] &= 0xF7;
	bWantInt = 1;
	pRam[0x24E] = KeyTable[key] | 0x80; // 保存按键信息到_KEYCODE
	pRam[0x04] |= 0x80; // 标记中断位
}

// done
BOOL KeybordInit()
{
	memset(KeyTable, 0xFF, sizeof(KeyTable));
	// 电脑按键映射到词典按键
	KeyTable[VK_NUMPAD0] = 0x31; // KEY_NUM_0
	KeyTable[VK_NUMPAD1] = 0x08; // KEY_NUM_1
	KeyTable[VK_NUMPAD2] = 0x09; // KEY_NUM_2
	KeyTable[VK_NUMPAD3] = 0x0A; // KEY_NUM_3
	KeyTable[VK_NUMPAD4] = 0x0B; // KEY_NUM_4
	KeyTable[VK_NUMPAD5] = 0x0C; // KEY_NUM_5
	KeyTable[VK_NUMPAD6] = 0x0D; // KEY_NUM_6
	KeyTable[VK_NUMPAD7] = 0x0E; // KEY_NUM_7
	KeyTable[VK_NUMPAD8] = 0x0F; // KEY_NUM_8
	KeyTable[VK_NUMPAD9] = 0x30; // KEY_NUM_9
	KeyTable['0'] = 0x31; // KEY_NUM_0
	KeyTable['1'] = 0x08; // KEY_NUM_1
	KeyTable['2'] = 0x09; // KEY_NUM_2
	KeyTable['3'] = 0x0A; // KEY_NUM_3
	KeyTable['4'] = 0x0B; // KEY_NUM_4
	KeyTable['5'] = 0x0C; // KEY_NUM_5
	KeyTable['6'] = 0x0D; // KEY_NUM_6
	KeyTable['7'] = 0x0E; // KEY_NUM_7
	KeyTable['8'] = 0x0F; // KEY_NUM_8
	KeyTable['9'] = 0x30; // KEY_NUM_9
	KeyTable['A'] = 0x18; // KEY_A
	KeyTable['B'] = 0x25; // KEY_B
	KeyTable['C'] = 0x23; // KEY_C
	KeyTable['D'] = 0x1A; // KEY_D
	KeyTable['E'] = 0x12; // KEY_E
	KeyTable['F'] = 0x1B; // KEY_F
	KeyTable['G'] = 0x1C; // KEY_G
	KeyTable['H'] = 0x1D; // KEY_H
	KeyTable['I'] = 0x17; // KEY_I
	KeyTable['J'] = 0x1E; // KEY_J
	KeyTable['K'] = 0x1F; // KEY_K
	KeyTable['L'] = 0x34; // KEY_L
	KeyTable['M'] = 0x27; // KEY_M
	KeyTable['N'] = 0x26; // KEY_N
	KeyTable['O'] = 0x32; // KEY_O
	KeyTable['P'] = 0x33; // KEY_P
	KeyTable['Q'] = 0x10; // KEY_Q
	KeyTable['R'] = 0x13; // KEY_R
	KeyTable['S'] = 0x19; // KEY_S
	KeyTable['T'] = 0x14; // KEY_T
	KeyTable['U'] = 0x16; // KEY_U
	KeyTable['V'] = 0x24; // KEY_V
	KeyTable['W'] = 0x11; // KEY_W
	KeyTable['X'] = 0x22; // KEY_X
	KeyTable['Y'] = 0x15; // KEY_Y
	KeyTable['Z'] = 0x21; // KEY_Z
	// 空格键
	KeyTable[VK_SPACE] = 0x36; // KEY_SPACE
	// ESC 键
	KeyTable[VK_ESCAPE] = 0x2E; // KEY_EXIT
	// Enter 键
	KeyTable[VK_RETURN] = 0x2F; // KEY_ENTER
	// 向上键
	KeyTable[VK_UP] = 0x35; // KEY_UP
	// 向左键
	KeyTable[VK_LEFT] = 0x37; // KEY_LEFT
	// 向下键
	KeyTable[VK_DOWN] = 0x38; // KEY_DOWN
	// 向右键
	KeyTable[VK_RIGHT] = 0x39; // KEY_RIGHT
	// PAGE UP 键
	KeyTable[VK_PRIOR] = 0x3A; // KEY_PGUP
	// PAGE DOWN 键
	KeyTable[VK_NEXT] = 0x3B; // KEY_PGDN

	// Pause Break 键
	KeyTable[VK_PAUSE] = 0x00; // KEY_ON_OFF
	// Tab 键
	KeyTable[VK_TAB] = 0x20; // KEY_INPUT 输入法
	// Ctrl 键
	KeyTable[VK_CONTROL] = 0x28; // KEY_ZY 中英符
	// HOME 键
	KeyTable[VK_HOME] = 0x01; // KEY_HOME_MENU 目录
	// INS 键
	KeyTable[VK_INSERT] = 0x02; // 双解
	// DEL 键
	KeyTable[VK_DELETE] = 0x03; // 现代
	// SHIFT 键
	KeyTable[VK_SHIFT] = 0x2D; // KEY_SHIFT
	return TRUE;
}

// done
BOOL InitLcd()
{
	bool fail;
	__try {
		hbmLcd = CreateBitmap(160, 96, 1, 1, NULL);
		fail = hbmLcd == NULL;
		if (fail)
		{
			__leave;
		}
		hdcLcd = CreateCompatibleDC(NULL);
		fail = hdcLcd == NULL;
		if (fail)
		{
			__leave;
		}
		hbmScreen = CreateBitmap(400, 192, 1, 1, NULL);
		fail = hbmScreen == NULL;
		if (fail)
		{
			__leave;
		}
		hdcScreen = CreateCompatibleDC(NULL);
		fail = hdcScreen == NULL;
		if (fail)
		{
			__leave;
		}
		hbmResource = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP));
		fail = hbmResource == NULL;
		if (fail)
		{
			__leave;
		}
		hdcResource = CreateCompatibleDC(NULL);
		fail = hdcResource == NULL;
	}
	__finally
	{
		if (fail)
		{
			if (hbmLcd)
			{
				DeleteObject(hbmLcd);
			}
			if (hdcLcd)
			{
				DeleteDC(hdcLcd);
			}
			if (hbmScreen)
			{
				DeleteObject(hbmScreen);
			}
			if (hdcScreen)
			{
				DeleteDC(hdcScreen);
			}
			if (hbmResource)
			{
				DeleteObject(hbmResource);
			}
			if (hdcResource)
			{
				DeleteDC(hdcResource);
			}
		}
		else
		{
			SelectObject(hdcLcd, hbmLcd);
			SelectObject(hdcResource, hbmResource);
			SelectObject(hdcScreen, hbmScreen);
		}
	}
	IconsInit();
	return !fail;
}

// done
BOOL LcdDestroy()
{
	if (hdcLcd)
	{
		DeleteDC(hdcLcd);
	}
	if (hbmLcd)
	{
		DeleteObject(hbmLcd);
	}
	if (hdcScreen)
	{
		DeleteDC(hdcScreen);
	}
	if (hbmScreen)
	{
		DeleteObject(hbmScreen);
	}
	if (hdcResource)
	{
		DeleteDC(hdcResource);
	}
	if (hbmResource)
	{
		DeleteObject(hbmResource);
	}
	return TRUE;
}

// 设置屏幕背景色
void SetScreenBackgroundColor(UINT8 v1)
{
	v1 = 0x3F - v1;
	v1 <<= 2;
	COLORREF color = RGB(v1, v1, v1);
	SetBkColor(hDC, color);
}

// 屏幕绘制
void ScreenPaint()
{
	int j;
	int i;
	pVideoMemory = pRam + 0x400;
	for (i = 0; i < 0x42; i++)
	{
		for (j = 0; j < 0x13; j++)
		{
			pvBitsLcd[(0x41 - i) * 20 + j + 1] = ~pVideoMemory[(i << 5) + j];
		}
	}
	for (i = 0x42; i < 0x60; i++)
	{
		for (j = 0; j < 0x13; j++)
		{
			pvBitsLcd[i * 20 + j + 1] = ~pVideoMemory[(i << 5) + j];
		}
	}
	for (i = 0; i < 0x41; i++)
	{
		pvBitsLcd[(0x40 - i) * 20] = ~pVideoMemory[(i << 5) + 19];
	}
	for (i = 0x41; i < 0x5F; i++)
	{
		pvBitsLcd[(i + 1) * 20] = ~pVideoMemory[(i << 5) + 19];
	}
	pvBitsLcd[0x514] = ~pVideoMemory[0xBF3];
	pvBitsLcd[0x515] = ~pVideoMemory[0xC00];
	SetBitmapBits(hbmLcd, 1920, pvBitsLcd);
	FillRect(hdcScreen, &LeftIconArea, (HBRUSH)GetStockObject(WHITE_BRUSH));
	FillRect(hdcScreen, &RightIconArea, (HBRUSH)GetStockObject(WHITE_BRUSH));
	StretchBlt(hdcScreen, 41, 0, 318, 192, hdcLcd, 0, 0, 159, 96, SRCCOPY);
	// 判断是否显示屏幕左右的图标
	for (i = 0; i < 0x60; i++)
	{
		if (pVideoMemory[0x12 + i * 0x20] & 0x01)
		{
			BitBlt(hdcScreen, rectResources[i][1].left,
				rectResources[i][1].top,
				rectResources[i][1].right - rectResources[i][1].left,
				rectResources[i][1].bottom - rectResources[i][1].top,
				hdcResource, rectResources[i][0].left, rectResources[i][0].top, SRCCOPY);
		}
	}
	int x = 0;
	int y = 0;
	LONG mult = screen_mult;
	if (IsZoomed(hWnd))
	{
		RECT rect;
		GetClientRect(hWnd, &rect);
		mult = (LONG)(ceil(__min((double)(rect.right - rect.left) / 400,
			(double)(rect.bottom - rect.top) / 192)));
		x = (rect.right - rect.left - 400 * mult) / 2;
		y = (rect.bottom - rect.top - 192 * mult) / 2;
	}
	StretchBlt(hDC, x, y, 400 * mult, 192 * mult,
		hdcScreen, 0, 0, 400, 192, SRCCOPY);
}

// done
void IconsInit()
{
	SetRect(&LeftIconArea, 0, 0, 41, 192);
	SetRect(&RightIconArea, 359, 0, 400, 192);
	// 0x0412 派生
	SetRect(&rectResources[0x00][0], 104, 32, 136, 48);
	SetRect(&rectResources[0x00][1], 364, 5 * 22 + 2, 396, 5 * 22 + 18);	//	364	2+22a	32	16
	// 0x0432 横线20
	SetRect(&rectResources[0x01][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x01][1], 2, 19 * 4 + 35, 9, 19 * 4 + 37); //	2	35+4a	7	2
	// 0x0452 横线19
	SetRect(&rectResources[0x02][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x02][1], 2, 18 * 4 + 35, 9, 18 * 4 + 37); //	2	35+4a	7	2
	// 0x0472 横线18
	SetRect(&rectResources[0x03][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x03][1], 2, 17 * 4 + 35, 9, 17 * 4 + 37); //	2	35+4a	7	2
	// 0x0492 横线17
	SetRect(&rectResources[0x04][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x04][1], 2, 16 * 4 + 35, 9, 16 * 4 + 37); //	2	35+4a	7	2
	// 0x04B2 横线16
	SetRect(&rectResources[0x05][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x05][1], 2, 15 * 4 + 35, 9, 15 * 4 + 37); //	2	35+4a	7	2
	// 0x04D2 扬声器
	SetRect(&rectResources[0x06][0], 54, 18, 71, 33);
	SetRect(&rectResources[0x06][1], 18, 105, 35, 120);	//	18	105	17	15
	// 0x04F2
	// 0x0512
	// 0x0532 横线15
	SetRect(&rectResources[0x09][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x09][1], 2, 14 * 4 + 35, 9, 14 * 4 + 37); //	2	35+4a	7	2
	// 0x0552 横线14
	SetRect(&rectResources[0x0A][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x0A][1], 2, 13 * 4 + 35, 9, 13 * 4 + 37); //	2	35+4a	7	2
	// 0x0572 横线13
	SetRect(&rectResources[0x0B][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x0B][1], 2, 12 * 4 + 35, 9, 12 * 4 + 37); //	2	35+4a	7	2
	// 0x0592 横线12
	SetRect(&rectResources[0x0C][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x0C][1], 2, 11 * 4 + 35, 9, 11 * 4 + 37); //	2	35+4a	7	2
	// 0x05B2 RingClock
	SetRect(&rectResources[0x0D][0], 39, 36, 58, 46);
	SetRect(&rectResources[0x0D][1], 14, 88, 36, 98);	//	14	88	22	10
	// 0x05D2 横线11
	SetRect(&rectResources[0x0E][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x0E][1], 2, 10 * 4 + 35, 9, 10 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x05F2 横线10
	SetRect(&rectResources[0x0F][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x0F][1], 2, 9 * 4 + 35, 9, 9 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0612 复合
	SetRect(&rectResources[0x10][0], 104, 16, 136, 32);
	SetRect(&rectResources[0x10][1], 364, 4 * 22 + 2, 396, 4 * 22 + 18);	//	364	2+22a	32	16
	// 0x0632 第一个数字的右上竖
	SetRect(&rectResources[0x11][0], 5, 1, 6, 5); // 数字右上竖
	SetRect(&rectResources[0x11][1], 9, 12, 10, 16);	//	9	12	1	4	(千位)数字右上竖
	// 0x0652 横线9
	SetRect(&rectResources[0x12][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x12][1], 2, 8 * 4 + 35, 9, 8 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0672 第一个数字的上横
	SetRect(&rectResources[0x13][0], 1, 0, 5, 1); // 数字上横
	SetRect(&rectResources[0x13][1], 5, 11, 9, 12);	//	5	11	4	1	(千位)数字上横
	// 0x0692 SHIFT
	SetRect(&rectResources[0x14][0], 9, 34, 34, 46); // 
	SetRect(&rectResources[0x14][1], 13, 69, 38, 81);	//	13	69	25	12	SHIFT
	// 0x06B2 横线8
	SetRect(&rectResources[0x15][0], 0, 18, 7, 20);
	SetRect(&rectResources[0x15][1], 2, 7 * 4 + 35, 9, 7 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x06D2 横线7
	SetRect(&rectResources[0x16][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x16][1], 2, 6 * 4 + 35, 9, 6 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x06F2 习语
	SetRect(&rectResources[0x17][0], 104, 0, 136, 16);
	SetRect(&rectResources[0x17][1], 364, 3 * 22 + 2, 396, 3 * 22 + 18);	//	364	2+22a	32	16
	// 0x0712 横线6
	SetRect(&rectResources[0x18][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x18][1], 2, 5 * 4 + 35, 9, 5 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0732 横线5
	SetRect(&rectResources[0x19][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x19][1], 2, 4 * 4 + 35, 9, 4 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0752 第一个数字的左上竖
	SetRect(&rectResources[0x1A][0], 0, 1, 1, 5); // 数字左上竖
	SetRect(&rectResources[0x1A][1], 4, 12, 5, 16);	//	4	12	1	4	(千位)数字左上竖
	// 0x0772 横线4
	SetRect(&rectResources[0x1B][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x1B][1], 2, 3 * 4 + 35, 9, 3 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0792 第一个数字的中横
	SetRect(&rectResources[0x1C][0], 1, 5, 5, 6); // 数字中横
	SetRect(&rectResources[0x1C][1], 5, 16, 9, 17);	//	5	16	4	1	(千位)数字中横
	// 0x07B2 例证
	SetRect(&rectResources[0x1D][0], 72, 32, 104, 48);
	SetRect(&rectResources[0x1D][1], 364, 2 * 22 + 2, 396, 2 * 22 + 18);	//	364	2+22a	32	16
	// 0x07D2 横线3
	SetRect(&rectResources[0x1E][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x1E][1], 2, 2 * 4 + 35, 9, 2 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x07F2 CAPS
	SetRect(&rectResources[0x1F][0], 9, 22, 34, 34); // 
	SetRect(&rectResources[0x1F][1], 13, 49, 38, 61);	//	13	49	25	12	CAPS
	// 0x0812 第一个数字的左下竖
	SetRect(&rectResources[0x20][0], 0, 6, 1, 10); // 数字左下竖
	SetRect(&rectResources[0x20][1], 4, 17, 5, 21);	//	4	17	1	4	(千位)数字左下竖
	// 0x0832 横线2
	SetRect(&rectResources[0x21][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x21][1], 2, 1 * 4 + 35, 9, 1 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0852 例句
	SetRect(&rectResources[0x22][0], 72, 16, 104, 32);
	SetRect(&rectResources[0x22][1], 364, 1 * 22 + 2, 396, 1 * 22 + 18);	//	364	2+22a	32	16
	// 0x0872 第一个数字的下横
	SetRect(&rectResources[0x23][0], 1, 10, 5, 11); // 数字下横
	SetRect(&rectResources[0x23][1], 5, 21, 9, 22);	//	5	21	4	1	(千位)数字下横
	// 0x0892 横线1
	SetRect(&rectResources[0x24][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x24][1], 2, 0 * 4 + 35, 9, 0 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x08B2 第一个数字的右下竖
	SetRect(&rectResources[0x25][0], 5, 6, 6, 10); // 数字右下竖
	SetRect(&rectResources[0x25][1], 9, 17, 10, 21);	//	9	17	1	4	(千位)数字右下竖
	// 0x08D2 滚动条上箭头
	SetRect(&rectResources[0x26][0], 0, 12, 7, 16);
	SetRect(&rectResources[0x26][1], 2, 29, 9, 33);	//	2	29	7	4	上
	// 0x08F2 第一个点
	SetRect(&rectResources[0x27][0], 7, 10, 8, 11); // 小数点
	SetRect(&rectResources[0x27][1], 11, 21, 12, 22);	//	11	21	1	1	小数点 todo位置
	// 0x0912 第二个数字的左下竖
	SetRect(&rectResources[0x28][0], 0, 6, 1, 10); // 数字左下竖
	SetRect(&rectResources[0x28][1], 13, 17, 14, 21);	//	13	17	1	4	(百位)数字左下竖
	// 0x0932 NUM
	SetRect(&rectResources[0x29][0], 9, 10, 34, 22); // 
	SetRect(&rectResources[0x29][1], 13, 29, 38, 41);	//	13	29	25	12	NUM
	// 0x0952 第二个数字的下横
	SetRect(&rectResources[0x2A][0], 1, 10, 5, 11); // 数字下横
	SetRect(&rectResources[0x2A][1], 14, 21, 18, 22);	//	14	21	4	1	(百位)数字下横
	// 0x0972 第二个数字的右下竖
	SetRect(&rectResources[0x2B][0], 5, 6, 6, 10); // 数字右下竖
	SetRect(&rectResources[0x2B][1], 18, 17, 19, 21);	//	18	17	1	4	(百位)数字右下竖
	// 0x0992 释义
	SetRect(&rectResources[0x2C][0], 72, 0, 104, 16);
	SetRect(&rectResources[0x2C][1], 364, 0 * 22 + 2, 396, 0 * 22 + 18);	//	364	2+22a	32	16
	// 0x09B2 第二个点
	SetRect(&rectResources[0x2D][0], 7, 10, 8, 11); // 小数点
	SetRect(&rectResources[0x2D][1], 20, 21, 21, 22);	//	20	21	1	1	小数点
	// 0x09D2 第三个数字的左下竖
	SetRect(&rectResources[0x2E][0], 0, 6, 1, 10); // 数字左下竖
	SetRect(&rectResources[0x2E][1], 22, 17, 23, 21);	//	22	17	1	4	(十位)数字左下竖
	// 0x09F2 第三个数字的下横
	SetRect(&rectResources[0x2F][0], 1, 10, 5, 11); // 数字下横
	SetRect(&rectResources[0x2F][1], 23, 21, 27, 22);	//	23	21	4	1	(十位)数字下横
	// 0x0A12 第三个数字的右下竖
	SetRect(&rectResources[0x30][0], 5, 6, 6, 10); // 数字右下竖
	SetRect(&rectResources[0x30][1], 27, 17, 28, 21);	//	27	17	1	4	(十位)数字右下竖
	// 0x0A32 第三个点
	SetRect(&rectResources[0x31][0], 7, 10, 8, 11); // 小数点
	SetRect(&rectResources[0x31][1], 29, 21, 30, 22);	//	29	21	1	1	小数点
	// 0x0A52 第四个数字的左下竖
	SetRect(&rectResources[0x32][0], 0, 6, 1, 10); // 数字左下竖
	SetRect(&rectResources[0x32][1], 31, 17, 32, 21);	//	31	17	1	4	(个位)数字左下竖
	// 0x0A72 第四个数字的下横
	SetRect(&rectResources[0x33][0], 1, 10, 5, 11); // 数字下横
	SetRect(&rectResources[0x33][1], 32, 21, 36, 22);	//	32	21	4	1	(个位)数字下横
	// 0x0A92 第四个数字的右下竖
	SetRect(&rectResources[0x34][0], 5, 6, 6, 10); // 数字右下竖
	SetRect(&rectResources[0x34][1], 36, 17, 37, 21);	//	36	17	1	4	(个位)数字右下竖
	// 0x0AB2 第四个数字的中横
	SetRect(&rectResources[0x35][0], 1, 5, 5, 6); // 数字中横
	SetRect(&rectResources[0x35][1], 32, 16, 36, 17);	//	32	16	4	1	(个位)数字中横
	// 0x0AD2 第四个数字的右上竖
	SetRect(&rectResources[0x36][0], 5, 1, 6, 5); // 数字右上竖
	SetRect(&rectResources[0x36][1], 36, 12, 37, 16);	//	36	12	1	4	(个位)数字右上竖
	// 0x0AF2 第四个数字的上横
	SetRect(&rectResources[0x37][0], 1, 0, 5, 1); // 数字上横
	SetRect(&rectResources[0x37][1], 32, 11, 36, 12);	//	32	11	4	1	(个位)数字上横
	// 0x0B12 第四个数字的左上竖
	SetRect(&rectResources[0x38][0], 0, 1, 1, 5); // 数字左上竖
	SetRect(&rectResources[0x38][1], 31, 12, 32, 16);	//	31	12	1	4	(个位)数字左上竖
	// 0x0B32 第三个数字的中横
	SetRect(&rectResources[0x39][0], 1, 5, 5, 6); // 数字中横
	SetRect(&rectResources[0x39][1], 23, 16, 27, 17);	//	23	16	4	1	(十位)数字中横
	// 0x0B52 第三个数字的右上竖
	SetRect(&rectResources[0x3A][0], 5, 1, 6, 5); // 数字右上竖
	SetRect(&rectResources[0x3A][1], 27, 12, 28, 16);	//	27	12	1	4	(十位)数字右上竖
	// 0x0B72 第三个数字的上横
	SetRect(&rectResources[0x3B][0], 1, 0, 5, 1); // 数字上横
	SetRect(&rectResources[0x3B][1], 23, 11, 27, 12);	//	23	11	4	1	(十位)数字上横
	// 0x0B92 第三个数字的左上竖
	SetRect(&rectResources[0x3C][0], 0, 1, 1, 5); // 数字左上竖
	SetRect(&rectResources[0x3C][1], 22, 12, 27, 16);	//	22	12	5	4	(十位)数字左上竖
	// 0x0BB2 双点
	SetRect(&rectResources[0x3D][0], 7, 3, 8, 8);
	SetRect(&rectResources[0x3D][1], 20, 14, 21, 19);	//	20	14	1	5
	// 0x0BD2 第二个数字的右上竖
	SetRect(&rectResources[0x3E][0], 5, 1, 6, 5); // 数字右上竖
	SetRect(&rectResources[0x3E][1], 18, 12, 19, 16);	//	18	12	1	4	(百位)数字右上竖
	// 0x0BF2 第二个数字的上横
	SetRect(&rectResources[0x3F][0], 1, 0, 5, 1); // 数字上横
	SetRect(&rectResources[0x3F][1], 14, 11, 18, 12);	//	14	11	4	1	(百位)数字上横
	// 0x0C12 第二个数字的左上竖
	SetRect(&rectResources[0x40][0], 0, 1, 1, 5); // 数字左上竖
	SetRect(&rectResources[0x40][1], 13, 12, 14, 16);	//	13	12	1	4	(百位)数字左上竖
	// 0x0C32 第二个数字的中横
	SetRect(&rectResources[0x41][0], 1, 5, 5, 6); // 数字中横
	SetRect(&rectResources[0x41][1], 14, 16, 18, 17);	//	14	16	4	1	(百位)数字中横
	// 0x0C52 横线21
	SetRect(&rectResources[0x42][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x42][1], 2, 20 * 4 + 35, 9, 20 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0C72 横线22
	SetRect(&rectResources[0x43][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x43][1], 2, 21 * 4 + 35, 9, 21 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0C92 Bell
	SetRect(&rectResources[0x44][0], 55, 0, 70, 14); // 闹钟
	SetRect(&rectResources[0x44][1], 17, 125, 32, 139);	//	17	125	15	14	闹钟开关
	// 0x0CB2 横线23
	SetRect(&rectResources[0x45][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x45][1], 2, 22 * 4 + 35, 9, 22 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0CD2 横线24
	SetRect(&rectResources[0x46][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x46][1], 2, 23 * 4 + 35, 9, 23 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0CF2 横线25
	SetRect(&rectResources[0x47][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x47][1], 2, 24 * 4 + 35, 9, 24 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0D12 横线26
	SetRect(&rectResources[0x48][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x48][1], 2, 25 * 4 + 35, 9, 25 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0D32 横线27
	SetRect(&rectResources[0x49][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x49][1], 2, 26 * 4 + 35, 9, 26 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0D52 横线28
	SetRect(&rectResources[0x4A][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x4A][1], 2, 27 * 4 + 35, 9, 27 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0D72 横线29
	SetRect(&rectResources[0x4B][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x4B][1], 2, 28 * 4 + 35, 9, 28 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0D92 上翻页
	SetRect(&rectResources[0x4C][0], 0, 28, 7, 36); // 
	SetRect(&rectResources[0x4C][1], 22, 144, 29, 152);	//	22	144	7	8	上翻页
	// 0x0DB2 横线30
	SetRect(&rectResources[0x4D][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x4D][1], 2, 29 * 4 + 35, 9, 29 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0DD2
	// 0x0DF2 横线31
	SetRect(&rectResources[0x4F][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x4F][1], 2, 30 * 4 + 35, 9, 30 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0E12 横线32
	SetRect(&rectResources[0x50][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x50][1], 2, 31 * 4 + 35, 9, 31 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0E32 横线33
	SetRect(&rectResources[0x51][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x51][1], 2, 32 * 4 + 35, 9, 32 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0E52 下翻页
	SetRect(&rectResources[0x52][0], 0, 38, 7, 46); // 
	SetRect(&rectResources[0x52][1], 22, 155, 29, 163);	//	22	155	7	8	下翻页
	// 0x0E72 横线34
	SetRect(&rectResources[0x53][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x53][1], 2, 33 * 4 + 35, 9, 33 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0E92 横线35
	SetRect(&rectResources[0x54][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x54][1], 2, 34 * 4 + 35, 9, 34 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0EB2 横线36
	SetRect(&rectResources[0x55][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x55][1], 2, 35 * 4 + 35, 9, 35 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0ED2 横线37
	SetRect(&rectResources[0x56][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x56][1], 2, 36 * 4 + 35, 9, 36 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0EF2 横线38
	SetRect(&rectResources[0x57][0], 0, 18, 7, 20); // 
	SetRect(&rectResources[0x57][1], 2, 37 * 4 + 35, 9, 37 * 4 + 37); //	2	35+4a	7	2 横线
	// 0x0F12 滚动条下箭头
	SetRect(&rectResources[0x58][0], 0, 22, 7, 26); // 
	SetRect(&rectResources[0x58][1], 2, 187, 9, 191);	//	2	187	7	4	下
	// 0x0F32 电池外框
	SetRect(&rectResources[0x59][0], 9, 0, 28, 8); // 模拟器不会低电量，使用满电池电量图标，也可降低电量图形合成难度
	SetRect(&rectResources[0x59][1], 16, 169, 36, 178);	//	16	169	20	8
	// 0x0F52 电池内电量2
	SetRect(&rectResources[0x5A][0], 9, 0, 28, 8); // 模拟器不会低电量，使用满电池电量图标，也可降低电量图形合成难度
	SetRect(&rectResources[0x5A][1], 16, 169, 36, 178);	//	16	169	20	8
	// 0x0F72 电池内电量1
	SetRect(&rectResources[0x5B][0], 9, 0, 28, 8); // 模拟器不会低电量，使用满电池电量图标，也可降低电量图形合成难度
	SetRect(&rectResources[0x5B][1], 16, 169, 36, 178);	//	16	169	20	8
	// 0x0F92 用法
	SetRect(&rectResources[0x5C][0], 136, 0, 168, 16);
	SetRect(&rectResources[0x5C][1], 364, 6 * 22 + 2, 396, 6 * 22 + 18);	//	364	2+22a	32	16
	// 0x0FB2 左箭头
	SetRect(&rectResources[0x5D][0], 36, 22, 51, 33);
	SetRect(&rectResources[0x5D][1], 18, 180, 33, 191);	//	18	180	15	11	左
	// 0x0FD2 同反
	SetRect(&rectResources[0x5E][0], 136, 16, 168, 32);
	SetRect(&rectResources[0x5E][1], 364, 7 * 22 + 2, 396, 7 * 22 + 18);	//	364	2+22a	32	16
	// 0x0FF2 右箭头
	SetRect(&rectResources[0x5F][0], 36, 10, 51, 21); // 
	SetRect(&rectResources[0x5F][1], 373, 180, 388, 191);	//	373	180	15	11	右
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
#ifdef _DEBUG
	InitOpCodePrintStrs();
#endif
	hInst = hInstance;
	if (!EMU_Init())
	{
		return 0;
	}
	EMU_MessageLoop();
	EMU_Destroy();

	return 0;
}

// done
BOOL EMU_Init()
{
	McuInit();
	McuReset();
	if (AllocMemory() == NULL)
	{
		MessageBox(NULL, "Alloc memory failure!", NULL, MB_OK);
		return FALSE;
	}
	if (LoadRom() == NULL)
	{
		ReleaseRom();
		MessageBox(NULL, "Load ROM failure!", NULL, MB_OK);
		return FALSE;
	}
	if (InitLcd() == NULL)
	{
		MessageBox(NULL, "Init LCD failure!", NULL, MB_OK);
		return FALSE;
	}
	FlashInit();
	KeybordInit();
	InitOpCodeMapping();
	ProgramInit();
	return TRUE;
}

// done
void EMU_Destroy()
{
	ReleaseWnd();
	ReleaseRom();
	LcdDestroy();
}

// done
void ReleaseWnd()
{
	ReleaseDC(hWnd, hDC);
	KillTimer(hWnd, 1);
	KillTimer(hWnd, 2);
}

struct FileNode {
	UINT8 addr_m;
	UINT8 addr_h;
	char name[0x0A];
	UINT8 size_l;
	UINT8 size_m;
	UINT8 size_h;
	UINT8 type;
};

void data2flash(char* path)
{
	char suffix_lower[5] = "";
	char* p = path;
	long index = -1;
	while (*p != '\0')
	{
		if (*p == '.')
		{
			index = p - path;
		}
		p++;
	}
	if (index < 0 || p - path - index != 4)
	{ // 没找到后缀或后缀(含.)不为4个字符
		return;
	}
	strncpy(suffix_lower, path + index, sizeof(suffix_lower));
	p = suffix_lower;
	// 大写字符转小写字符
	while (*p != '\0')
	{
		if (*p >= 'A' && *p <= 'Z')
		{
			*p += 0x20;
		}
		p++;
	}
	UINT8 file_type = 0xFE;
	UINT8 _A917[0x10] = {
		0x3F, // 电子图书
		0x3A, // 开机动画
		0x11, // 开机铃声
		0x3B, // 词典类
		0x35, // 背诵类
		0x39, // 学习类
		0x36, // 计算类
		0x3E, // 资料类
		0x37, // 名片类
		0x38, // 时间类
		0x33, // 下载类
		0x3D, // 游戏类
		0x34, // 系统类
		0x2D, // 听力类
		0x2C, // 声音类
		0x2B, // 静态图片
	};
	if (strncmp(suffix_lower, ".txt", sizeof(suffix_lower) - 1) == 0)
	{ // 电子图书
		file_type = 0x3F;
	}
	else if (strncmp(suffix_lower, ".dct", sizeof(suffix_lower) - 1) == 0)
	{ // 下载词典
		file_type = 0x3B;
	}
	else if (strncmp(suffix_lower, ".dlg", sizeof(suffix_lower) - 1) == 0)
	{ // 有声读物
		file_type = 0x2D;
	}
	else if (strncmp(suffix_lower, ".gam", sizeof(suffix_lower) - 1) == 0)
	{ // 迷你游戏
		file_type = 0x3D;
	}
	else if (strncmp(suffix_lower, ".std", sizeof(suffix_lower) - 1) == 0)
	{ // 学习资料
		file_type = 0x39;
	}
	else if (strncmp(suffix_lower, ".dat", sizeof(suffix_lower) - 1) == 0)
	{ // 电子资料
		file_type = 0x3E;
	}
	else if (strncmp(suffix_lower, ".rct", sizeof(suffix_lower) - 1) == 0)
	{ // 背诵词库
		file_type = 0x35;
	}
	else if (strncmp(suffix_lower, ".prg", sizeof(suffix_lower) - 1) == 0)
	{ // 下载程序
		file_type = 0x33;
	}
	else if (strncmp(suffix_lower, ".ani", sizeof(suffix_lower) - 1) == 0)
	{ // 开机动画
		file_type = 0x3A;
	}
	else if (strncmp(suffix_lower, ".bmp", sizeof(suffix_lower) - 1) == 0)
	{ // 开机图片
		file_type = 0x2B;
	}
	else if (strncmp(suffix_lower, ".snd", sizeof(suffix_lower) - 1) == 0)
	{ // 开机音效
		file_type = 0x2C;
	}
	else if (strncmp(suffix_lower, ".sub", sizeof(suffix_lower) - 1) == 0)
	{ // 黄冈试题
		file_type = 0x33;
	}
	else if (strncmp(suffix_lower, ".poi", sizeof(suffix_lower) - 1) == 0)
	{ // 黄冈课程
		file_type = 0x33;
	}
	else if (strncmp(suffix_lower, ".bsc", sizeof(suffix_lower) - 1) == 0)
	{ // Bsc 程序
		file_type = 0x3C;
	}
	else
	{ // 不支持的格式
		return;
	}
	UINT16 free_node_addr = 0x0000;
	for (UINT16 i = 0; i < 0x200; i++)
	{
		if (pFlashMemory[free_node_addr + 0x0F] == 0x00
			|| pFlashMemory[free_node_addr + 0x0F] == 0xFF)
		{
			break;
		}
		free_node_addr += 0x10;
	}
	if (free_node_addr >= 0x2000)
	{ // 没有FileNode空间了
		return;
	}
	OFSTRUCT of = {};
	DWORD dwFileSizeHigh;
	HANDLE hDataFile = (HANDLE)OpenFile(path, &of, OF_READ);
	DWORD dwFileSizeLow = GetFileSize(hDataFile, &dwFileSizeHigh);
	UINT8* data_buff = (UINT8*)malloc(dwFileSizeLow);
	DWORD dwNumberOfBytesRead = 0;
	ReadFile(hDataFile, data_buff, dwFileSizeLow, &dwNumberOfBytesRead, NULL);
	CloseHandle(hDataFile);
	UINT16 databag_num = (dwFileSizeLow + 0xFFF) >> 12;
	struct FileNode fileNode;
	int free_sector = 0;
	UINT32 free_start_addr = 0x0000;
	for (int sector = 0x0F; sector < 0x1F0; sector++)
	{
		if ((sector < 0x100 && pFlashMemory[0x1000 + sector] == 0xFF)
			|| (sector >= 0x100 && pFlashMemory[0x8000 + sector - 0x100] == 0xFF))
		{
			free_sector++;
			if (free_sector == databag_num)
			{
				free_start_addr = (sector - (databag_num - 1)) << 4;
				fileNode.addr_m = free_start_addr;
				fileNode.addr_h = free_start_addr >> 8;
				for (int i = 0; i < databag_num; i++)
				{
					if (sector - (databag_num - i - 1) >= 0x100)
					{
						pFlashMemory[0x8000 + sector - (databag_num - i - 1) - 0x100] = 0x01;
					}
					else
					{
						pFlashMemory[0x1000 + sector - (databag_num - i - 1)] = 0x01;
					}
				}
				break;
			}
		}
		else
		{
			free_sector = 0;
		}
	}
	if (free_sector != databag_num)
	{ // flash 空间不够
		free(data_buff);
		MessageBox(hWnd, "请通过模拟器删除数据后再试。", "Flash存储空间不够", MB_OK);
		return;
	}
	memcpy(fileNode.name, data_buff + 6, 0x0A);
	fileNode.size_l = dwFileSizeLow;
	fileNode.size_m = dwFileSizeLow >> 8;
	fileNode.size_h = dwFileSizeLow >> 16;
	fileNode.type = file_type;
	memcpy(pFlashMemory + free_node_addr, &fileNode, sizeof(FileNode));
	memcpy(pFlashMemory + (free_start_addr << 8), data_buff, dwFileSizeLow);
	free(data_buff);
}

// done
void ProgramInit()
{
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInst;
	wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BA4988));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = NULL;// (HBRUSH)GetStockObject(GRAY_BRUSH);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szTitle;
	wcex.hIconSm = NULL;
	RegisterClassEx(&wcex);

	RECT rect = { 0, 0, 400 * screen_mult, 192 * screen_mult };
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_ACCEPTFILES);
	frame_width = rect.right - rect.left - 400 * screen_mult;
	frame_height = rect.bottom - rect.top - 192 * screen_mult;
	hWnd = CreateWindowEx(WS_EX_ACCEPTFILES, szTitle, szTitle,
		WS_OVERLAPPEDWINDOW,
		(GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2,
		(GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2,
		rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInst, nullptr);
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
	hDC = GetDC(hWnd);
	SetBkColor(hDC, RGB(0xE0, 0xE0, 0xE0)); // 0x5C5D54
	SetTimer(hWnd, 1, 100, NULL);
	SetTimer(hWnd, 2, 1000, NULL);
}


//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		DragAcceptFiles(hWnd, TRUE);
		return 0;
	case WM_DROPFILES:
	{
		HDROP hDrop = (HDROP)wParam;
		UINT nFileNum = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		char strFileName[MAX_PATH];
		for (UINT i = 0; i < nFileNum; i++)
		{
			DragQueryFile(hDrop, i, strFileName, MAX_PATH);
			data2flash(strFileName);
		}
	}
	return 0;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_F12:
			McuReset();
			SleepFlag = 0;
			pRam[0x200] &= 0xF7;
			break;
		default:
			KeyResponse(LOWORD(wParam));
			break;
		}
		return 0;
	case WM_TIMER:
		switch (wParam)
		{
		case 1:
			ScreenPaint();
			break;
		case 2:
			McuRtcImplement();
			break;
		}
		break;
	case WM_SIZING:
	{
		RECT* wrect = (RECT*)lParam;
		FIXED_WINDOW fixed = FIEXD_NONE;

		int h = wrect->bottom - wrect->top;
		int w = wrect->right - wrect->left;
		//GetClientRect;
		double mult = 1.0;
		switch (wParam)
		{
		case WMSZ_LEFT:
		case WMSZ_RIGHT:
			mult = (double)(w - frame_width) / 400;
			break;
		case WMSZ_TOP:
		case WMSZ_BOTTOM:
			mult = (double)(h - frame_height) / 192;
			break;
		default:
			mult = __max((double)(w - frame_width) / 400,
				(double)(h - frame_height) / 192);
			break;
		}
		screen_mult = __max(1, LONG(round(mult)));
		switch (wParam)
		{
		case WMSZ_LEFT:
			fixed = FIEXD_RIGHTCENTER;
			break;
		case WMSZ_RIGHT:
			fixed = FIEXD_LEFTCENTER;
			break;
		case WMSZ_TOP:
			fixed = FIEXD_BOTTOMCENTER;
			break;
		case WMSZ_BOTTOM:
			fixed = FIEXD_TOPCENTER;
			break;
		case WMSZ_BOTTOMRIGHT:
			fixed = FIEXD_TOPLEFT;
			break;
		case WMSZ_TOPRIGHT:
			fixed = FIEXD_BOTTOMLEFT;
			break;
		case WMSZ_TOPLEFT:
			fixed = FIEXD_BOTTOMRIGHT;
			break;
		case WMSZ_BOTTOMLEFT:
			fixed = FIEXD_TOPRIGHT;
			break;
		}
		CalcWindowSize(wrect, fixed);
		break;
	}
	case WM_PAINT: // Paint the main window
		ScreenPaint();
		break;
	case WM_DESTROY: // post a quit message and return
		PostQuitMessage(0);
		break;
	case WM_ERASEBKGND:
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// done
BOOL EMU_MessageLoop()
{
	MSG msg;
	for (;;)
	{
		if (CycleCounter > 40000)
		{
			McuTimerImplement(100);
			CycleCounter = 0;
		}
		if (PeekMessage(&msg, NULL, WM_NULL, WM_NULL, PM_REMOVE))
		{
			if (msg.message == 0x12)
			{
				return FALSE;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (SleepFlag)
			{
				Sleep(10);
				McuTimerImplement(100);
			}
			else
			{
				McuStep();
				McuStep();
			}
		}
	}
}

// done
BOOL LoadRom()
{
	UINT8* fileAddr;
	CHAR str[0x80]; // ReturnBuffer
	CHAR errMsg[0x80];
	CHAR IniFilePath[260]; // IniFilePath
	UINT8* realAddr;
	UINT32 segs;
	CHAR FileName[260];
	UINT32 bins;
	UINT32 size; // 数据长度
	CHAR KeyName[0x10];
	UINT32 offset; // 偏移量
	UINT32 i;
	UINT32 segAddr; // 数据映射到基址
	CHAR AppName[0x10];
	UINT32 channel;

	UpdateProgramDirectory();
	strcpy(IniFilePath, ProgramDirectory);
	strcat(IniFilePath, "4988.ini");
	bins = 0;
	for (;;)
	{
		sprintf(AppName, "BIN%02d", bins);
		FileName[0] = '\0';
		GetPrivateProfileString(AppName, "FileName", NULL, FileName, sizeof(FileName), IniFilePath);
		if (FileName[0] == '\0')
		{
			break;
		}
		fileAddr = DoFileMapping(FileName, 1);
		if (fileAddr == NULL)
		{
			return 0;
		}
		segs = 0;
		for (;;)
		{
			sprintf(KeyName, "Segment%02d", segs);
			str[0] = '\0';
			GetPrivateProfileString(AppName, KeyName, NULL, str, sizeof(str), IniFilePath);
			if (str[0] == '\0')
			{
				break;
			}
			sscanf(str, "%x,%x,%x,%x", &segAddr, &offset, &size, &channel);
			if (segAddr >= 0x1000000 || channel < 1 || size < offset)
			{
				sprintf(errMsg, "INI参数错误：[%s] - %s", AppName, KeyName);
				MessageBox(NULL, errMsg, NULL, MB_OK);
				return FALSE;
			}
			realAddr = fileAddr + offset;
			offset >>= 0x13;
			size >>= 0x13;
			segAddr >>= 0x13;
			channel -= 1;
			for (i = 0; i <= size - offset; i++)
			{
				segPageChannelBaseAddrs[segAddr + i][channel] = realAddr;
				segPageBaseAddrs[segAddr + i] = segPageChannelBaseAddrs[segAddr + i][0];
				realAddr += 0x80000;
			}
			segs += 1;
		}
		bins += 1;
	}
	str[0] = '\0';
	GetPrivateProfileString("FLASH", "StartAdr", NULL, str, sizeof(str), IniFilePath);
	if (str[0] != '\0')
	{
		sscanf(str, "%x", &FlashStartAdr);
	}
	str[0] = '\0';
	GetPrivateProfileString("FLASH", "Size", NULL, str, sizeof(str), IniFilePath);
	if (str[0] != '\0')
	{
		sscanf(str, "%x", &FlashSize);
	}
	FlashEndAdr = FlashStartAdr + FlashSize;
	pFlashMemory = segPageBaseAddrs[FlashStartAdr >> 0x13];
	if (pFlashMemory == NULL)
	{
		MessageBox(NULL, "未配置FLASH", NULL, MB_OK);
		return FALSE;
	}
	// 复制boot部分代码到0x300处
	memcpy(pRam + 0x300, segPageBaseAddrs[0x1F] + 0x7FF00, 0x100);
	return TRUE;
}

// done
void ReleaseRom()
{
	ReleaseFileMappings();
	if (pRam != NULL)
	{
		UINT8* p = pRam;
		free(p);
		pRam = NULL;
	}
}

// 获取程序所在目录
void UpdateProgramDirectory()
{
	char* str = ProgramDirectory;
	char* pos = str;
	GetModuleFileName(NULL, ProgramDirectory, 260);
	while (*str)
	{
		if ((*str) == '\\')
		{
			pos = str;
		}
		str++;
	}
	*(pos + 1) = '\0';
}

// done
void ReadOpData()
{
	__try
	{
		if ((visitFlashAddr < FlashEndAdr) && (visitFlashAddr >= FlashStartAdr))
		{
			opData = ReadFlash(visitFlashAddr - FlashStartAdr);
		}
		else
		{
			opData = segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFFF];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "读无效地址:%08x\nPC=%04X(%06X) PB=%02X", visitFlashAddr, cpu.PC, errorAddr, pRam[0x21B]);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
}

// 写数据
void WriteOpData()
{
	__try
	{
		if ((visitFlashAddr < FlashEndAdr) && (visitFlashAddr >= FlashStartAdr))
		{
			WriteFlash(visitFlashAddr - FlashStartAdr, opData);
		}
		else
		{
			segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFFF] = opData;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "写无效地址:%08x\nPC=%04X(%06X) PB=%02X", visitFlashAddr, cpu.PC, errorAddr, pRam[0x21B]);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
}

// done
void SwitchSegPageChannel()
{
	segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFF] = opData;
	UINT32 channel = opData >> 6;
	for (INT32 i = 0; i < 8; i++)
	{
		segPageBaseAddrs[i + 8] = segPageChannelBaseAddrs[i + 8][channel];
	}
}

// done
void ClearScreen()
{
	segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFF] = opData & 0x3F;
	if ((opData & 0xC0) == 0x80)
	{
		SetScreenBackgroundColor((UINT8)opData & 0x3F);
	}
}

// done
BOOL AllocMemory()
{
	memset(segPageBaseAddrs, 0, sizeof(segPageBaseAddrs));
	memset(segPageChannelBaseAddrs, 0, sizeof(segPageChannelBaseAddrs));
	pRam = (UINT8*)malloc(0x8000);
	if (NULL == pRam)
	{
		return FALSE;
	}
	memset(pRam, 0, 0x8000);
	segPageBaseAddrs[0] = pRam;
	FlashSize = 0x200000; // 2MB
	FlashStartAdr = 0x200000;
	FlashEndAdr = FlashStartAdr + FlashSize;
	McuRegesterInit();
	WriteRegisterFns[0x21B] = SwitchSegPageChannel; // _PB
	//WriteRegisterFns[0x218] = ClearScreen; // _PA
	return AllocFileMemory();
}

// done
void AddressingModeNop()
{
	// VOID
}

// done
void AddressingModeImmediate()
{
	visitAddr = cpu.PC++;
}

// done
void AddressingModeAbsolute()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 2;
	visitAddr = *(UINT16*)&segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
}

// done
void AddressingModeNone()
{
	visitAddr = cpu.PC++;
}

// done
void AddressingModeIndirect()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 1;
	opData = segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
	visitAddr = *(UINT16*)(pRam + opData);
}

// done
void AddressingModeAbsoluteX()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 2;
	visitAddr = *(UINT16*)&segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
	visitAddr += cpu.X;
}

// done
void AddressingModeAbsoluteY()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 2;
	visitAddr = *(UINT16*)&segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
	visitAddr += cpu.Y;
}

// done
void AddressingModeZeroPage()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 1;
	visitAddr = segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
}

// done
void AddressingModeZeroPageX()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 1;
	visitAddr = segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
	visitAddr += cpu.X;
	visitAddr &= 0xFF;
}

// done
void AddressingModeZeroPageY()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 1;
	visitAddr = segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
	visitAddr += cpu.Y;
	visitAddr &= 0xFF;
}

// done
void AddressingModeIndirectX()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 1;
	visitAddr = segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
	visitAddr += cpu.X;
	visitAddr &= 0xFF;
	visitAddr = *(UINT16*)&pRam[visitAddr];
}

// done
void AddressingModeIndirectY()
{
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	cpu.PC += 1;
	visitAddr = segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF];
	visitAddr = *(UINT16*)&pRam[visitAddr];
	visitAddr += cpu.Y;
}

// done
void InstructionAdc()
{
	LoadOpData();
	INT32 data = cpu.A;
	data += opData;
	data += (cpu.P & 0x01);
	if (data > 0xFF)
	{
		cpu.P |= 0x40;
	}
	else
	{
		cpu.P &= 0xBF;
	}
	if (data > 0xFF)
	{
		cpu.P |= 0x01;
	}
	else
	{
		cpu.P &= 0xFE;
	}
	cpu.A = data & 0xFF;
	if (cpu.P & 0x08)
	{
		cpu.P &= 0xFE;
		if ((cpu.A & 0x0F) > 0x09)
		{
			cpu.A += 0x06;
		}
		if ((cpu.A & 0xF0) > 0x90)
		{
			cpu.A += 0x60;
			cpu.P |= 0x01;
		}
	}
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// AND
void InstructionAnd()
{
	LoadOpData();
	cpu.A &= opData;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionAsl()
{
	LoadOpData();
	cpu.P = (cpu.P & 0xFE) | ((opData >> 7) & 0x01);
	opData <<= 1;
	opData &= 0xFF;
	SaveOpData();
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionAsl_A()
{
	cpu.P = (cpu.P & 0xFE) | ((cpu.A >> 7) & 0x01);
	cpu.A <<= 1;
	cpu.A &= 0xFF;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionBbr0()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x01) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbr1()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x02) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbr2()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x04) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbr3()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x08) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbr4()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x10) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbr5()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x20) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbr6()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x40) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbr7()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if ((data & 0x80) == 0x00)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs0()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x01)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs1()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x02)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs2()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x04)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs3()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x08)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs4()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x10)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs5()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x20)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs6()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x40)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBbs7()
{
	LoadOpData();
	UINT32 data = *(UINT32*)(pRam + opData);
	LoadOpData();
	if (data & 0x80)
	{
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBcc()
{
	if ((cpu.P & 0x01) == 0x00)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBcs()
{
	if (cpu.P & 0x01)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBeq()
{
	if (cpu.P & 0x02)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBit()
{
	LoadOpData();
	if (opData & cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	cpu.P = (cpu.P & 0x3F) | (opData & 0xC0);
}

// done
void InstructionBmi()
{
	if (cpu.P & 0x80)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBne()
{
	if ((cpu.P & 0x02) == 0x00)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBpl()
{
	if ((cpu.P & 0x80) == 0x00)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBra()
{
	LoadOpData();
	cpu.PC += opData;
	if (opData > 0x7F)
	{
		cpu.PC -= 0x100;
	}
}

// done
void InstructionUnknown()
{
	char msg[0x80];
#ifdef _DEBUG
	debug_save_regedits();
#endif
	cpu.PC--;
	errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	sprintf(msg, "PC=%04X(%06X) PB=%02X", cpu.PC, errorAddr, pRam[0x21B]);
	MessageBox(NULL, msg, "程序跑飞!", MB_OK);
	McuReset();
}

// done
void InstructionBvc()
{
	if ((cpu.P & 0x40) == 0x00)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionBvs()
{
	if (cpu.P & 0x40)
	{
		LoadOpData();
		cpu.PC += opData;
		if (opData > 0x7F)
		{
			cpu.PC -= 0x100;
		}
	}
}

// done
void InstructionClc()
{
	cpu.P &= 0xFE;
}

// done
void InstructionCld()
{
	cpu.P &= 0xF7;
}

// done
void InstructionCli()
{
	cpu.P &= 0xFB;
}

// done
void InstructionClv()
{
	cpu.P &= 0xBF;
}

// done
void InstructionCmp()
{
	LoadOpData();
	if ((cpu.A + 0x100 - opData) > 0xFF)
	{
		cpu.P |= 0x01;
	}
	else
	{
		cpu.P &= 0xFE;
	}
	opData = (cpu.A + 0x100 - opData) & 0xFF;
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionCpx()
{
	LoadOpData();
	if ((cpu.X + 0x100 - opData) > 0xFF)
	{
		cpu.P |= 0x01;
	}
	else
	{
		cpu.P &= 0xFE;
	}
	opData = (cpu.X + 0x100 - opData) & 0xFF;
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionCpy()
{
	LoadOpData();
	if ((cpu.Y + 0x100 - opData) > 0xFF)
	{
		cpu.P |= 0x01;
	}
	else
	{
		cpu.P &= 0xFE;
	}
	opData = (cpu.Y + 0x100 - opData) & 0xFF;
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionDec_A()
{
	cpu.A--;
	cpu.A &= 0xFF;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionDec()
{
	LoadOpData();
	opData--;
	SaveOpData();
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionDex()
{
	cpu.X--;
	cpu.X &= 0xFF;
	if (cpu.X)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.X & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionDey()
{
	cpu.Y--;
	cpu.Y &= 0xFF;
	if (cpu.Y)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.Y & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionEor()
{
	LoadOpData();
	cpu.A ^= opData;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionInc_A()
{
	cpu.A++;
	cpu.A &= 0xFF;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionInc()
{
	LoadOpData();
	opData++;
	opData &= 0xFF;
	SaveOpData();
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionInx()
{
	cpu.X++;
	cpu.X &= 0xFF;
	if (cpu.X)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.X & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionIny()
{
	cpu.Y++;
	cpu.Y &= 0xFF;
	if (cpu.Y)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.Y & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionJmp()
{
	cpu.PC = visitAddr;
}

// done
void InstructionJsr()
{
	UINT32 pc = cpu.PC;
	cpu.PC = visitAddr;
	pc--;
	cpu.S -= 2;
	*(UINT16*)(pRam + cpu.S + 1) = pc;
#ifdef _DEBUG
	pc -= 2;
	stack_list[stack_list_index] = BankAddressList[pc >> 0x0C] | (pc & 0x0FFF);
	stack_list_index++;
	if (cpu.PC == 0xD2F6)
	{
		stack_list[stack_list_index] = *(UINT16*)(pRam + 0x26);
		stack_list_index++;
	}
#endif
}

// done
void InstructionLda()
{
	LoadOpData();
	cpu.A = opData;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionLdx()
{
	LoadOpData();
	cpu.X = opData;
	if (cpu.X)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.X & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionLdy()
{
	LoadOpData();
	cpu.Y = opData;
	if (cpu.Y)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.Y & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionLsr()
{
	LoadOpData();
	cpu.P = (cpu.P & 0xFE) | (opData & 0x01);
	opData >>= 1;
	SaveOpData();
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionLsr_A()
{
	cpu.P = (cpu.P & 0xFE) | (cpu.A & 0x01);
	cpu.A >>= 1;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// NOP
void InstructionNop()
{
	// VOID
}

// done
void InstructionOra()
{
	LoadOpData();
	cpu.A |= opData;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionPha()
{
	pRam[cpu.S] = cpu.A;
	cpu.S--;
}

// done
void InstructionPhp()
{
	pRam[cpu.S] = cpu.P;
	cpu.S--;
}

// done
void InstructionPhx()
{
	pRam[cpu.S] = cpu.X;
	cpu.S--;
}

// done
void InstructionPhy()
{
	pRam[cpu.S] = cpu.Y;
	cpu.S--;
}

// done
void InstructionPla()
{
	cpu.S++;
	cpu.A = pRam[cpu.S];
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionPlp()
{
	cpu.S++;
	cpu.P = pRam[cpu.S];
}

// done
void InstructionPlx()
{
	cpu.S++;
	cpu.X = pRam[cpu.S];
	if (cpu.X)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.X & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionPly()
{
	cpu.S++;
	cpu.Y = pRam[cpu.S];
	if (cpu.Y)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.Y & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionRol()
{
	LoadOpData();
	UINT32 cBit = cpu.P & 0x01;
	cpu.P = (cpu.P & 0xFE) | ((opData >> 0x07) & 0x01);
	opData <<= 1;
	opData |= cBit;
	SaveOpData();
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionRol_A()
{
	UINT32 cBit = cpu.P & 0x01;
	cpu.P = (cpu.P & 0xFE) | ((cpu.A >> 0x07) & 0x01);
	cpu.A <<= 1;
	cpu.A &= 0xFF;
	cpu.A |= cBit;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionRor()
{
	LoadOpData();
	UINT32 cBit = cpu.P & 0x01;
	cpu.P = (cpu.P & 0xFE) | (opData & 0x01);
	opData >>= 1;
	if (cBit)
	{
		opData |= 0x80;
	}
	SaveOpData();
	if (opData)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (opData & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionRor_A()
{
	UINT32 cBit = cpu.P & 0x01;
	cpu.P = (cpu.P & 0xFE) | (cpu.A & 0x01);
	cpu.A >>= 1;
	if (cBit)
	{
		cpu.A |= 0x80;
	}
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionRti()
{
	UINT32 data = *(UINT32*)(pRam + cpu.S + 1);
	cpu.P = data & 0xFF;
	cpu.PC = (data >> 8) & 0xFFFF;
	cpu.S += 3;
}

// done
void InstructionRts()
{
	cpu.PC = *(UINT16*)(pRam + cpu.S + 1) + 1;
	cpu.S += 2;
#ifdef _DEBUG
	stack_list_index--;
#endif
}

// done
void InstructionSbc()
{
	LoadOpData();
	opData ^= 0xFF;
	UINT32 cBit = cpu.P & 0x01;
	INT32 data = (INT8)cpu.A + (INT8)opData + (cBit << 4);
	if ((data > 0x7F) || (data < (-0x80)))
	{
		cpu.P |= 0x40;
	}
	else
	{
		cpu.P &= 0xBF;
	}
	data = cpu.A + opData + cBit;
	if (data > 0xFF)
	{
		cpu.P |= 0x01;
	}
	else
	{
		cpu.P &= 0xFE;
	}
	cpu.A = data & 0xFF;
	if (cpu.P & 0x08)
	{
		cpu.A -= 0x66;
		cpu.A &= 0xFF;
		cpu.P &= 0xFE;
		if ((cpu.A & 0x0F) > 0x09)
		{
			cpu.A += 0x06;
		}
		if ((cpu.A & 0xF0) > 0x90)
		{
			cpu.A += 0x60;
			cpu.A &= 0xFF;
			cpu.P |= 0x01;
		}
	}
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionSec()
{
	cpu.P |= 0x01;
}

// done
void InstructionSed()
{
	cpu.P |= 0x08;
}

// done
void InstructionSei()
{
	cpu.P |= 0x04;
}

// done
void InstructionSta()
{
	opData = cpu.A;
	SaveOpData();
}

// done
void InstructionStx()
{
	opData = cpu.X;
	SaveOpData();
}

// done
void InstructionSty()
{
	opData = cpu.Y;
	SaveOpData();
}

// done
void InstructionStz()
{
	opData = 0;
	SaveOpData();
}

// done
void InstructionTax()
{
	cpu.X = cpu.A;
	if (cpu.X)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.X & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionTay()
{
	cpu.Y = cpu.A;
	if (cpu.Y)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.Y & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionTsx()
{
	cpu.X = cpu.S & 0xFF;
	if (cpu.X)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.X & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionTxa()
{
	cpu.A = cpu.X;
	if (cpu.A)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.A & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void InstructionTxs()
{
	cpu.S = cpu.X | 0x100;
}

// done
void InstructionTya()
{
	cpu.A = cpu.Y;
	if (cpu.Y)
	{
		cpu.P &= 0xFD;
	}
	else
	{
		cpu.P |= 0x02;
	}
	if (cpu.Y & 0x80)
	{
		cpu.P |= 0x80;
	}
	else
	{
		cpu.P &= 0x7F;
	}
}

// done
void ReadRegister()
{
	opData = segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFFF];
}

// done
void ReadAddr1()
{
	UINT32 _ADDR1 = *(UINT32*)(pRam + 0x208) & 0xFFFFFF;
	__try
	{
		if ((_ADDR1 < FlashEndAdr) && (_ADDR1 >= FlashStartAdr))
		{
			opData = ReadFlash(_ADDR1 - FlashStartAdr);
		}
		else
		{
			opData = segPageBaseAddrs[_ADDR1 >> 0x13][_ADDR1 & 0x7FFFF];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		sprintf(msg, "读无效地址:%06x,可能的原因:字库配置不正确", _ADDR1);
		MessageBox(NULL, msg, NULL, MB_OK);
		PostQuitMessage(0);
	}
	if ((pRam[0x207] & 0x01) == 0)
	{
		return;
	}
	pRam[0x208]++;
	if (pRam[0x208] == 0)
	{
		pRam[0x209]++;
		if (pRam[0x209] == 0)
		{
			pRam[0x20A]++;
		}
	}
}

// done
void ReadAddr2()
{
	UINT32 _ADDR2 = *(UINT32*)(pRam + 0x20B) & 0xFFFFFF;
	__try
	{
		if ((_ADDR2 < FlashEndAdr) && (_ADDR2 >= FlashStartAdr))
		{
			opData = ReadFlash(_ADDR2 - FlashStartAdr);
		}
		else
		{
			opData = segPageBaseAddrs[_ADDR2 >> 0x13][_ADDR2 & 0x7FFFF];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "读无效地址:%08x\nPC=%04X(%06X)", _ADDR2, cpu.PC, errorAddr);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
	if ((pRam[0x207] & 0x02) == 0)
	{
		return;
	}
	pRam[0x20B]++;
	if (pRam[0x20B] == 0)
	{
		pRam[0x20C]++;
		if (pRam[0x20C] == 0)
		{
			pRam[0x20D]++;
		}
	}
}

// done
void ReadAddr3()
{
	UINT32 _ADDR3 = *(UINT32*)(pRam + 0x20E) & 0xFFFFFF;
	__try
	{
		if ((_ADDR3 < FlashEndAdr) && (_ADDR3 >= FlashStartAdr))
		{
			opData = ReadFlash(_ADDR3 - FlashStartAdr);
		}
		else
		{
			opData = segPageBaseAddrs[_ADDR3 >> 0x13][_ADDR3 & 0x7FFFF];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "读无效地址:%08x\nPC=%04X(%06X)", _ADDR3, cpu.PC, errorAddr);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
	if ((pRam[0x207] & 0x04) == 0)
	{
		return;
	}
	pRam[0x20E]++;
	if (pRam[0x20E] == 0)
	{
		pRam[0x20F]++;
		if (pRam[0x20F] == 0)
		{
			pRam[0x210]++;
		}
	}
}

// done
void ReadAddr4()
{
	UINT32 _ADDR4 = *(UINT32*)(pRam + 0x211) & 0xFFFFFF;
	__try
	{
		if ((_ADDR4 < FlashEndAdr) && (_ADDR4 >= FlashStartAdr))
		{
			opData = ReadFlash(_ADDR4 - FlashStartAdr);
		}
		else
		{
			opData = segPageBaseAddrs[_ADDR4 >> 0x13][_ADDR4 & 0x7FFFF];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "读无效地址:%08x\nPC=%04X(%06X)", _ADDR4, cpu.PC, errorAddr);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
	if ((pRam[0x207] & 0x08) == 0)
	{
		return;
	}
	pRam[0x211]++;
	if (pRam[0x211] == 0)
	{
		pRam[0x212]++;
		if (pRam[0x212] == 0)
		{
			pRam[0x213]++;
		}
	}
}

// done
void ReadBkSel()
{
	opData = BankSelected;
}

// done
void ReadBkAdrL()
{
	opData = BankAddressList[BankSelected] >> 0x0C;
	opData &= 0xFF;
}

// done
void ReadBkAdrH()
{
	opData = BankAddressList[BankSelected] >> 0x14;
	opData &= 0xFF;
}

// done
void ReadUrCon1()
{
	opData = segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFF];
}

// done
void ReadSBuf()
{
	// VOID
}

// done
void WriteRegister()
{
	segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFF] = opData;
}

// done
void WriteAddr1()
{
	UINT32 _ADDR1 = *(UINT32*)(pRam + 0x208) & 0xFFFFFF;
	__try
	{
		if ((_ADDR1 < FlashEndAdr) && (_ADDR1 >= FlashStartAdr))
		{
			WriteFlash(_ADDR1 - FlashStartAdr, opData);
		}
		else
		{
			segPageBaseAddrs[_ADDR1 >> 0x13][_ADDR1 & 0x7FFFF] = opData;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "写无效地址:%08x\nPC=%04X(%06X)", _ADDR1, cpu.PC, errorAddr);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
	if ((pRam[0x207] & 0x01) == 0)
	{
		return;
	}
	pRam[0x208]++;
	if (pRam[0x208] == 0)
	{
		pRam[0x209]++;
		if (pRam[0x209] == 0)
		{
			pRam[0x20A]++;
		}
	}
}

// done
void WriteAddr2()
{
	UINT32 _ADDR2 = *(UINT32*)(pRam + 0x20B) & 0xFFFFFF;
	__try
	{
		if ((_ADDR2 < FlashEndAdr) && (_ADDR2 >= FlashStartAdr))
		{
			WriteFlash(_ADDR2 - FlashStartAdr, opData);
		}
		else
		{
			segPageBaseAddrs[_ADDR2 >> 0x13][_ADDR2 & 0x7FFFF] = opData;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "写无效地址:%08x\nPC=%04X(%06X)", _ADDR2, cpu.PC, errorAddr);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
	if ((pRam[0x207] & 0x02) == 0)
	{
		return;
	}
	pRam[0x20B]++;
	if (pRam[0x20B] == 0)
	{
		pRam[0x20C]++;
		if (pRam[0x20C] == 0)
		{
			pRam[0x20D]++;
		}
	}
}

// done
void WriteAddr3()
{
	UINT32 _ADDR3 = *(UINT32*)(pRam + 0x20E) & 0xFFFFFF;
	__try
	{
		if ((_ADDR3 < FlashEndAdr) && (_ADDR3 >= FlashStartAdr))
		{
			WriteFlash(_ADDR3 - FlashStartAdr, opData);
		}
		else
		{
			segPageBaseAddrs[_ADDR3 >> 0x13][_ADDR3 & 0x7FFFF] = opData;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "写无效地址:%08x\nPC=%04X(%06X)", _ADDR3, cpu.PC, errorAddr);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
	if ((pRam[0x207] & 0x04) == 0)
	{
		return;
	}
	pRam[0x20E]++;
	if (pRam[0x20E] == 0)
	{
		pRam[0x20F]++;
		if (pRam[0x20F] == 0)
		{
			pRam[0x210]++;
		}
	}
}

// done
void WriteAddr4()
{
	UINT32 _ADDR4 = *(UINT32*)(pRam + 0x211) & 0xFFFFFF;
	__try
	{
		if ((_ADDR4 < FlashEndAdr) && (_ADDR4 >= FlashStartAdr))
		{
			WriteFlash(_ADDR4 - FlashStartAdr, opData);
		}
		else
		{
			segPageBaseAddrs[_ADDR4 >> 0x13][_ADDR4 & 0x7FFFF] = opData;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[0x80];
		errorAddr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
		sprintf(msg, "写无效地址:%08x\nPC=%04X(%06X)", _ADDR4, cpu.PC, errorAddr);
		MessageBox(NULL, msg, NULL, MB_OK);
	}
	if ((pRam[0x207] & 0x08) == 0)
	{
		return;
	}
	pRam[0x211]++;
	if (pRam[0x211] == 0)
	{
		pRam[0x212]++;
		if (pRam[0x212] == 0)
		{
			pRam[0x213]++;
		}
	}
}

// done
void WriteBkSel()
{
	BankSelected = opData;
}

// done
void WriteBkAdrL()
{
	opData &= 0xFF;
	BankAddressList[BankSelected] &= 0xFF00000; //保护高位
	BankAddressList[BankSelected] |= opData << 0x0C;
}

// done
void WriteBkAdrH()
{
	opData &= 0xFF;
	if (opData > 0x0F)
	{
		opData &= 0x0F;
	}
	BankAddressList[BankSelected] &= 0xFF000; // 保护低位
	BankAddressList[BankSelected] |= opData << 0x14;
}

// done
void WriteIsr()
{
	pRam[visitFlashAddr] &= opData;
}

// done
void WriteTIsr()
{
	pRam[visitFlashAddr] &= opData;
}

// done
void WriteSysCon()
{
	if (opData & 0x08)
	{
		SleepFlag = 1;
	}
	segPageBaseAddrs[visitFlashAddr >> 0x13][visitFlashAddr & 0x7FFFF] = opData;
}

// done
void WriteUsCon1()
{
	WriteRegister();
}

// done
void WriteSBuf()
{
	// VOID
}

void McuNoInterruption()
{
	// VOID
}

// 操作码和寄存器功能映射
void McuInit()
{
	pInterruptHandle[0] = McuNoInterruption;
	pInterruptHandle[1] = McuInterruptHandle;

	// BRK
	CyclesNumberTable[0x00] = 0x07;
	pInstruction[0x00] = InstructionUnknown;
	pAddressingMode[0x00] = AddressingModeNop;
	// ORA - (Indirect,X)
	CyclesNumberTable[0x01] = 0x06;
	pInstruction[0x01] = InstructionOra;
	pAddressingMode[0x01] = AddressingModeIndirectX;
	// Future Expansion
	CyclesNumberTable[0x02] = 0x02;
	pInstruction[0x02] = InstructionUnknown;
	pAddressingMode[0x02] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x03] = 0x02;
	pInstruction[0x03] = InstructionUnknown;
	pAddressingMode[0x03] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x04] = 0x03;
	pInstruction[0x04] = InstructionUnknown;
	pAddressingMode[0x04] = AddressingModeNop;
	// ORA - Zero Page
	CyclesNumberTable[0x05] = 0x03;
	pInstruction[0x05] = InstructionOra;
	pAddressingMode[0x05] = AddressingModeZeroPage;
	// ASL - Zero Page
	CyclesNumberTable[0x06] = 0x05;
	pInstruction[0x06] = InstructionAsl;
	pAddressingMode[0x06] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0x07] = 0x02;
	pInstruction[0x07] = InstructionUnknown;
	pAddressingMode[0x07] = AddressingModeNop;
	// PHP
	CyclesNumberTable[0x08] = 0x03;
	pInstruction[0x08] = InstructionPhp;
	pAddressingMode[0x08] = AddressingModeNop;
	// ORA - Immediate
	CyclesNumberTable[0x09] = 0x03;
	pInstruction[0x09] = InstructionOra;
	pAddressingMode[0x09] = AddressingModeImmediate;
	// ASL - Accumulator
	CyclesNumberTable[0x0A] = 0x02;
	pInstruction[0x0A] = InstructionAsl_A;
	pAddressingMode[0x0A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x0B] = 0x02;
	pInstruction[0x0B] = InstructionUnknown;
	pAddressingMode[0x0B] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x0C] = 0x04;
	pInstruction[0x0C] = InstructionUnknown;
	pAddressingMode[0x0C] = AddressingModeNop;
	// ORA - Absolute
	CyclesNumberTable[0x0D] = 0x04;
	pInstruction[0x0D] = InstructionOra;
	pAddressingMode[0x0D] = AddressingModeAbsolute;
	// ASL - Absolute
	CyclesNumberTable[0x0E] = 0x06;
	pInstruction[0x0E] = InstructionAsl;
	pAddressingMode[0x0E] = AddressingModeAbsolute;
	// BBR0
	CyclesNumberTable[0x0F] = 0x02;
	pInstruction[0x0F] = InstructionBbr0;
	pAddressingMode[0x0F] = AddressingModeNone;
	// BPL
	CyclesNumberTable[0x10] = 0x02;
	pInstruction[0x10] = InstructionBpl;
	pAddressingMode[0x10] = AddressingModeNone;
	// ORA - (Indirect),Y
	CyclesNumberTable[0x11] = 0x05;
	pInstruction[0x11] = InstructionOra;
	pAddressingMode[0x11] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0x12] = 0x03;
	pInstruction[0x12] = InstructionUnknown;
	pAddressingMode[0x12] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x13] = 0x02;
	pInstruction[0x13] = InstructionUnknown;
	pAddressingMode[0x13] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x14] = 0x03;
	pInstruction[0x14] = InstructionUnknown;
	pAddressingMode[0x14] = AddressingModeNop;
	// ORA - Zero Page,X
	CyclesNumberTable[0x15] = 0x04;
	pInstruction[0x15] = InstructionOra;
	pAddressingMode[0x15] = AddressingModeZeroPageX;
	// ASL - Zero Page,X
	CyclesNumberTable[0x16] = 0x06;
	pInstruction[0x16] = InstructionAsl;
	pAddressingMode[0x16] = AddressingModeZeroPageX;
	// Future Expansion
	CyclesNumberTable[0x17] = 0x02;
	pInstruction[0x17] = InstructionUnknown;
	pAddressingMode[0x17] = AddressingModeNop;
	// CLC
	CyclesNumberTable[0x18] = 0x02;
	pInstruction[0x18] = InstructionClc;
	pAddressingMode[0x18] = AddressingModeNop;
	// ORA - Absolute,Y
	CyclesNumberTable[0x19] = 0x04;
	pInstruction[0x19] = InstructionOra;
	pAddressingMode[0x19] = AddressingModeAbsoluteY;
	// INC - Accumulator
	CyclesNumberTable[0x1A] = 0x02;
	pInstruction[0x1A] = InstructionInc_A;
	pAddressingMode[0x1A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x1B] = 0x02;
	pInstruction[0x1B] = InstructionUnknown;
	pAddressingMode[0x1B] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x1C] = 0x04;
	pInstruction[0x1C] = InstructionUnknown;
	pAddressingMode[0x1C] = AddressingModeNop;
	// ORA - Absolute,X
	CyclesNumberTable[0x1D] = 0x04;
	pInstruction[0x1D] = InstructionOra;
	pAddressingMode[0x1D] = AddressingModeAbsoluteX;
	// ASL - Absolute,X
	CyclesNumberTable[0x1E] = 0x07;
	pInstruction[0x1E] = InstructionAsl;
	pAddressingMode[0x1E] = AddressingModeAbsoluteX;
	// BBR1
	CyclesNumberTable[0x1F] = 0x02;
	pInstruction[0x1F] = InstructionBbr1;
	pAddressingMode[0x1F] = AddressingModeNone;
	// JSR
	CyclesNumberTable[0x20] = 0x06;
	pInstruction[0x20] = InstructionJsr;
	pAddressingMode[0x20] = AddressingModeAbsolute;
	// AND - (Indirect,X)
	CyclesNumberTable[0x21] = 0x06;
	pInstruction[0x21] = InstructionAnd;
	pAddressingMode[0x21] = AddressingModeIndirectX;
	// Future Expansion
	CyclesNumberTable[0x22] = 0x02;
	pInstruction[0x22] = InstructionUnknown;
	pAddressingMode[0x22] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x23] = 0x02;
	pInstruction[0x23] = InstructionUnknown;
	pAddressingMode[0x23] = AddressingModeNop;
	// BIT - Zero Page
	CyclesNumberTable[0x24] = 0x03;
	pInstruction[0x24] = InstructionBit;
	pAddressingMode[0x24] = AddressingModeZeroPage;
	// AND - Zero Page
	CyclesNumberTable[0x25] = 0x03;
	pInstruction[0x25] = InstructionAnd;
	pAddressingMode[0x25] = AddressingModeZeroPage;
	// ROL - Zero Page
	CyclesNumberTable[0x26] = 0x05;
	pInstruction[0x26] = InstructionRol;
	pAddressingMode[0x26] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0x27] = 0x02;
	pInstruction[0x27] = InstructionUnknown;
	pAddressingMode[0x27] = AddressingModeNop;
	// PLP
	CyclesNumberTable[0x28] = 0x04;
	pInstruction[0x28] = InstructionPlp;
	pAddressingMode[0x28] = AddressingModeNop;
	// AND - Immediate
	CyclesNumberTable[0x29] = 0x03;
	pInstruction[0x29] = InstructionAnd;
	pAddressingMode[0x29] = AddressingModeImmediate;
	// ROL - Accumulator
	CyclesNumberTable[0x2A] = 0x02;
	pInstruction[0x2A] = InstructionRol_A;
	pAddressingMode[0x2A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x2B] = 0x02;
	pInstruction[0x2B] = InstructionUnknown;
	pAddressingMode[0x2B] = AddressingModeNop;
	// BIT - Absolute
	CyclesNumberTable[0x2C] = 0x04;
	pInstruction[0x2C] = InstructionBit;
	pAddressingMode[0x2C] = AddressingModeAbsolute;
	// AND - Absolute
	CyclesNumberTable[0x2D] = 0x04;
	pInstruction[0x2D] = InstructionAnd;
	pAddressingMode[0x2D] = AddressingModeAbsolute;
	// ROL - Absolute
	CyclesNumberTable[0x2E] = 0x06;
	pInstruction[0x2E] = InstructionRol;
	pAddressingMode[0x2E] = AddressingModeAbsolute;
	// BBR2
	CyclesNumberTable[0x2F] = 0x02;
	pInstruction[0x2F] = InstructionBbr2;
	pAddressingMode[0x2F] = AddressingModeNone;
	// BMI
	CyclesNumberTable[0x30] = 0x02;
	pInstruction[0x30] = InstructionBmi;
	pAddressingMode[0x30] = AddressingModeNone;
	// AND - (Indirect),Y
	CyclesNumberTable[0x31] = 0x05;
	pInstruction[0x31] = InstructionAnd;
	pAddressingMode[0x31] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0x32] = 0x03;
	pInstruction[0x32] = InstructionUnknown;
	pAddressingMode[0x32] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x33] = 0x02;
	pInstruction[0x33] = InstructionUnknown;
	pAddressingMode[0x33] = AddressingModeNop;
	// BIT - Zero Page,X
	CyclesNumberTable[0x34] = 0x04;
	pInstruction[0x34] = InstructionBit;
	pAddressingMode[0x34] = AddressingModeZeroPageX;
	// AND - Zero Page,X
	CyclesNumberTable[0x35] = 0x04;
	pInstruction[0x35] = InstructionAnd;
	pAddressingMode[0x35] = AddressingModeZeroPageX;
	// ROL - Zero Page,X
	CyclesNumberTable[0x36] = 0x06;
	pInstruction[0x36] = InstructionRol;
	pAddressingMode[0x36] = AddressingModeZeroPageX;
	// Future Expansion
	CyclesNumberTable[0x37] = 0x02;
	pInstruction[0x37] = InstructionUnknown;
	pAddressingMode[0x37] = AddressingModeNop;
	// SEC
	CyclesNumberTable[0x38] = 0x02;
	pInstruction[0x38] = InstructionSec;
	pAddressingMode[0x38] = AddressingModeNop;
	// AND - Absolute,Y
	CyclesNumberTable[0x39] = 0x04;
	pInstruction[0x39] = InstructionAnd;
	pAddressingMode[0x39] = AddressingModeAbsoluteY;
	// DEC - Accumulator
	CyclesNumberTable[0x3A] = 0x02;
	pInstruction[0x3A] = InstructionDec_A;
	pAddressingMode[0x3A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x3B] = 0x02;
	pInstruction[0x3B] = InstructionUnknown;
	pAddressingMode[0x3B] = AddressingModeNop;
	// BIT - Absolute,X
	CyclesNumberTable[0x3C] = 0x04;
	pInstruction[0x3C] = InstructionBit;
	pAddressingMode[0x3C] = AddressingModeAbsoluteX;
	// AND - Absolute,X
	CyclesNumberTable[0x3D] = 0x04;
	pInstruction[0x3D] = InstructionAnd;
	pAddressingMode[0x3D] = AddressingModeAbsoluteX;
	// ROL - Absolute,X
	CyclesNumberTable[0x3E] = 0x07;
	pInstruction[0x3E] = InstructionRol;
	pAddressingMode[0x3E] = AddressingModeAbsoluteX;
	// BBR3
	CyclesNumberTable[0x3F] = 0x02;
	pInstruction[0x3F] = InstructionBbr3;
	pAddressingMode[0x3F] = AddressingModeNone;
	// RTI
	CyclesNumberTable[0x40] = 0x06;
	pInstruction[0x40] = InstructionRti;
	pAddressingMode[0x40] = AddressingModeNop;
	// EOR - (Indirect,X)
	CyclesNumberTable[0x41] = 0x06;
	pInstruction[0x41] = InstructionEor;
	pAddressingMode[0x41] = AddressingModeIndirectX;
	// Future Expansion
	CyclesNumberTable[0x42] = 0x02;
	pInstruction[0x42] = InstructionUnknown;
	pAddressingMode[0x42] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x43] = 0x02;
	pInstruction[0x43] = InstructionUnknown;
	pAddressingMode[0x43] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x44] = 0x02;
	pInstruction[0x44] = InstructionUnknown;
	pAddressingMode[0x44] = AddressingModeNop;
	// EOR - Zero Page
	CyclesNumberTable[0x45] = 0x03;
	pInstruction[0x45] = InstructionEor;
	pAddressingMode[0x45] = AddressingModeZeroPage;
	// LSR - Zero Page
	CyclesNumberTable[0x46] = 0x05;
	pInstruction[0x46] = InstructionLsr;
	pAddressingMode[0x46] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0x47] = 0x02;
	pInstruction[0x47] = InstructionUnknown;
	pAddressingMode[0x47] = AddressingModeNop;
	// PHA
	CyclesNumberTable[0x48] = 0x03;
	pInstruction[0x48] = InstructionPha;
	pAddressingMode[0x48] = AddressingModeNop;
	// EOR - Immediate
	CyclesNumberTable[0x49] = 0x03;
	pInstruction[0x49] = InstructionEor;
	pAddressingMode[0x49] = AddressingModeImmediate;
	// LSR - Accumulator
	CyclesNumberTable[0x4A] = 0x02;
	pInstruction[0x4A] = InstructionLsr_A;
	pAddressingMode[0x4A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x4B] = 0x02;
	pInstruction[0x4B] = InstructionUnknown;
	pAddressingMode[0x4B] = AddressingModeNop;
	// JMP - Absolute
	CyclesNumberTable[0x4C] = 0x03;
	pInstruction[0x4C] = InstructionJmp;
	pAddressingMode[0x4C] = AddressingModeAbsolute;
	// EOR - Absolute
	CyclesNumberTable[0x4D] = 0x04;
	pInstruction[0x4D] = InstructionEor;
	pAddressingMode[0x4D] = AddressingModeAbsolute;
	// LSR - Absolute
	CyclesNumberTable[0x4E] = 0x06;
	pInstruction[0x4E] = InstructionLsr;
	pAddressingMode[0x4E] = AddressingModeAbsolute;
	// BBR4
	CyclesNumberTable[0x4F] = 0x02;
	pInstruction[0x4F] = InstructionBbr4;
	pAddressingMode[0x4F] = AddressingModeNone;
	// BVC
	CyclesNumberTable[0x50] = 0x02;
	pInstruction[0x50] = InstructionBvc;
	pAddressingMode[0x50] = AddressingModeNone;
	// EOR - (Indirect),Y
	CyclesNumberTable[0x51] = 0x05;
	pInstruction[0x51] = InstructionEor;
	pAddressingMode[0x51] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0x52] = 0x03;
	pInstruction[0x52] = InstructionUnknown;
	pAddressingMode[0x52] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x53] = 0x02;
	pInstruction[0x53] = InstructionUnknown;
	pAddressingMode[0x53] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x54] = 0x02;
	pInstruction[0x54] = InstructionUnknown;
	pAddressingMode[0x54] = AddressingModeNop;
	// EOR - Zero Page,X
	CyclesNumberTable[0x55] = 0x04;
	pInstruction[0x55] = InstructionEor;
	pAddressingMode[0x55] = AddressingModeZeroPageX;
	// LSR - Zero Page,X
	CyclesNumberTable[0x56] = 0x06;
	pInstruction[0x56] = InstructionLsr;
	pAddressingMode[0x56] = AddressingModeZeroPageX;
	// Future Expansion
	CyclesNumberTable[0x57] = 0x02;
	pInstruction[0x57] = InstructionUnknown;
	pAddressingMode[0x57] = AddressingModeNop;
	// CLI
	CyclesNumberTable[0x58] = 0x02;
	pInstruction[0x58] = InstructionCli;
	pAddressingMode[0x58] = AddressingModeNop;
	// EOR - Absolute,Y
	CyclesNumberTable[0x59] = 0x04;
	pInstruction[0x59] = InstructionEor;
	pAddressingMode[0x59] = AddressingModeAbsoluteY;
	// PHY
	CyclesNumberTable[0x5A] = 0x03;
	pInstruction[0x5A] = InstructionPhy;
	pAddressingMode[0x5A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x5B] = 0x02;
	pInstruction[0x5B] = InstructionUnknown;
	pAddressingMode[0x5B] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x5C] = 0x02;
	pInstruction[0x5C] = InstructionUnknown;
	pAddressingMode[0x5C] = AddressingModeNop;
	// EOR - Absolute,X
	CyclesNumberTable[0x5D] = 0x04;
	pInstruction[0x5D] = InstructionEor;
	pAddressingMode[0x5D] = AddressingModeAbsoluteX;
	// LSR - Absolute,X
	CyclesNumberTable[0x5E] = 0x07;
	pInstruction[0x5E] = InstructionLsr;
	pAddressingMode[0x5E] = AddressingModeAbsoluteX;
	// BBR5
	CyclesNumberTable[0x5F] = 0x02;
	pInstruction[0x5F] = InstructionBbr5;
	pAddressingMode[0x5F] = AddressingModeNone;
	// RTS
	CyclesNumberTable[0x60] = 0x06;
	pInstruction[0x60] = InstructionRts;
	pAddressingMode[0x60] = AddressingModeNop;
	// ADC - (Indirect,X)
	CyclesNumberTable[0x61] = 0x06;
	pInstruction[0x61] = InstructionAdc;
	pAddressingMode[0x61] = AddressingModeIndirectX;
	// Future Expansion
	CyclesNumberTable[0x62] = 0x02;
	pInstruction[0x62] = InstructionUnknown;
	pAddressingMode[0x62] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x63] = 0x02;
	pInstruction[0x63] = InstructionUnknown;
	pAddressingMode[0x63] = AddressingModeNop;
	// STZ - Zero Page
	CyclesNumberTable[0x64] = 0x03;
	pInstruction[0x64] = InstructionStz;
	pAddressingMode[0x64] = AddressingModeZeroPage;
	// ADC - Zero Page
	CyclesNumberTable[0x65] = 0x03;
	pInstruction[0x65] = InstructionAdc;
	pAddressingMode[0x65] = AddressingModeZeroPage;
	// ROR - Zero Page
	CyclesNumberTable[0x66] = 0x05;
	pInstruction[0x66] = InstructionRor;
	pAddressingMode[0x66] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0x67] = 0x02;
	pInstruction[0x67] = InstructionUnknown;
	pAddressingMode[0x67] = AddressingModeNop;
	// PLA
	CyclesNumberTable[0x68] = 0x04;
	pInstruction[0x68] = InstructionPla;
	pAddressingMode[0x68] = AddressingModeNop;
	// ADC - Immediate
	CyclesNumberTable[0x69] = 0x03;
	pInstruction[0x69] = InstructionAdc;
	pAddressingMode[0x69] = AddressingModeImmediate;
	// ROR - Accumulator
	CyclesNumberTable[0x6A] = 0x02;
	pInstruction[0x6A] = InstructionRor_A;
	pAddressingMode[0x6A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x6B] = 0x02;
	pInstruction[0x6B] = InstructionUnknown;
	pAddressingMode[0x6B] = AddressingModeNop;
	// JMP - Indirect
	CyclesNumberTable[0x6C] = 0x05;
	pInstruction[0x6C] = InstructionJmp;
	pAddressingMode[0x6C] = AddressingModeIndirect;
	// ADC - Absolute
	CyclesNumberTable[0x6D] = 0x04;
	pInstruction[0x6D] = InstructionAdc;
	pAddressingMode[0x6D] = AddressingModeAbsolute;
	// ROR - Absolute
	CyclesNumberTable[0x6E] = 0x06;
	pInstruction[0x6E] = InstructionRor;
	pAddressingMode[0x6E] = AddressingModeAbsolute;
	// BBR6
	CyclesNumberTable[0x6F] = 0x02;
	pInstruction[0x6F] = InstructionBbr6;
	pAddressingMode[0x6F] = AddressingModeNone;
	// BVS
	CyclesNumberTable[0x70] = 0x02;
	pInstruction[0x70] = InstructionBvs;
	pAddressingMode[0x70] = AddressingModeNone;
	// ADC - (Indirect),Y
	CyclesNumberTable[0x71] = 0x05;
	pInstruction[0x71] = InstructionAdc;
	pAddressingMode[0x71] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0x72] = 0x03;
	pInstruction[0x72] = InstructionUnknown;
	pAddressingMode[0x72] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x73] = 0x02;
	pInstruction[0x73] = InstructionUnknown;
	pAddressingMode[0x73] = AddressingModeNop;
	// STZ - Zero Page,X
	CyclesNumberTable[0x74] = 0x04;
	pInstruction[0x74] = InstructionStz;
	pAddressingMode[0x74] = AddressingModeZeroPageX;
	// ADC - Zero Page,X
	CyclesNumberTable[0x75] = 0x04;
	pInstruction[0x75] = InstructionAdc;
	pAddressingMode[0x75] = AddressingModeZeroPageX;
	// ROR - Zero Page,X
	CyclesNumberTable[0x76] = 0x06;
	pInstruction[0x76] = InstructionRor;
	pAddressingMode[0x76] = AddressingModeZeroPageX;
	// Future Expansion
	CyclesNumberTable[0x77] = 0x02;
	pInstruction[0x77] = InstructionUnknown;
	pAddressingMode[0x77] = AddressingModeNop;
	// SEI
	CyclesNumberTable[0x78] = 0x02;
	pInstruction[0x78] = InstructionSei;
	pAddressingMode[0x78] = AddressingModeNop;
	// ADC - Absolute,Y
	CyclesNumberTable[0x79] = 0x04;
	pInstruction[0x79] = InstructionAdc;
	pAddressingMode[0x79] = AddressingModeAbsoluteY;
	// PLY
	CyclesNumberTable[0x7A] = 0x04;
	pInstruction[0x7A] = InstructionPly;
	pAddressingMode[0x7A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x7B] = 0x02;
	pInstruction[0x7B] = InstructionUnknown;
	pAddressingMode[0x7B] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x7C] = 0x06;
	pInstruction[0x7C] = InstructionUnknown;
	pAddressingMode[0x7C] = AddressingModeNop;
	// ADC - Absolute,X
	CyclesNumberTable[0x7D] = 0x04;
	pInstruction[0x7D] = InstructionAdc;
	pAddressingMode[0x7D] = AddressingModeAbsoluteX;
	// ROR - Absolute,X
	CyclesNumberTable[0x7E] = 0x07;
	pInstruction[0x7E] = InstructionRor;
	pAddressingMode[0x7E] = AddressingModeAbsoluteX;
	// BBR7
	CyclesNumberTable[0x7F] = 0x02;
	pInstruction[0x7F] = InstructionBbr7;
	pAddressingMode[0x7F] = AddressingModeNone;
	// BRA
	CyclesNumberTable[0x80] = 0x03;
	pInstruction[0x80] = InstructionBra;
	pAddressingMode[0x80] = AddressingModeNone;
	// STA - (Indirect,X)
	CyclesNumberTable[0x81] = 0x06;
	pInstruction[0x81] = InstructionSta;
	pAddressingMode[0x81] = AddressingModeIndirectX;
	// Future Expansion
	CyclesNumberTable[0x82] = 0x02;
	pInstruction[0x82] = InstructionUnknown;
	pAddressingMode[0x82] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x83] = 0x02;
	pInstruction[0x83] = InstructionUnknown;
	pAddressingMode[0x83] = AddressingModeNop;
	// STY - Zero Page
	CyclesNumberTable[0x84] = 0x02;
	pInstruction[0x84] = InstructionSty;
	pAddressingMode[0x84] = AddressingModeZeroPage;
	// STA - Zero Page
	CyclesNumberTable[0x85] = 0x02;
	pInstruction[0x85] = InstructionSta;
	pAddressingMode[0x85] = AddressingModeZeroPage;
	// STX - Zero Page
	CyclesNumberTable[0x86] = 0x02;
	pInstruction[0x86] = InstructionStx;
	pAddressingMode[0x86] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0x87] = 0x02;
	pInstruction[0x87] = InstructionUnknown;
	pAddressingMode[0x87] = AddressingModeNop;
	// DEY
	CyclesNumberTable[0x88] = 0x02;
	pInstruction[0x88] = InstructionDey;
	pAddressingMode[0x88] = AddressingModeNop;
	// BIT - Immediate
	CyclesNumberTable[0x89] = 0x02;
	pInstruction[0x89] = InstructionBit;
	pAddressingMode[0x89] = AddressingModeImmediate;
	// TXA
	CyclesNumberTable[0x8A] = 0x02;
	pInstruction[0x8A] = InstructionTxa;
	pAddressingMode[0x8A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x8B] = 0x02;
	pInstruction[0x8B] = InstructionUnknown;
	pAddressingMode[0x8B] = AddressingModeNop;
	// STY - Absolute
	CyclesNumberTable[0x8C] = 0x04;
	pInstruction[0x8C] = InstructionSty;
	pAddressingMode[0x8C] = AddressingModeAbsolute;
	// STA - Absolute
	CyclesNumberTable[0x8D] = 0x04;
	pInstruction[0x8D] = InstructionSta;
	pAddressingMode[0x8D] = AddressingModeAbsolute;
	// STX - Absolute
	CyclesNumberTable[0x8E] = 0x04;
	pInstruction[0x8E] = InstructionStx;
	pAddressingMode[0x8E] = AddressingModeAbsolute;
	// BBS0
	CyclesNumberTable[0x8F] = 0x02;
	pInstruction[0x8F] = InstructionBbs0;
	pAddressingMode[0x8F] = AddressingModeNone;
	// BCC
	CyclesNumberTable[0x90] = 0x02;
	pInstruction[0x90] = InstructionBcc;
	pAddressingMode[0x90] = AddressingModeNone;
	// STA - (Indirect),Y
	CyclesNumberTable[0x91] = 0x06;
	pInstruction[0x91] = InstructionSta;
	pAddressingMode[0x91] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0x92] = 0x03;
	pInstruction[0x92] = InstructionUnknown;
	pAddressingMode[0x92] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x93] = 0x02;
	pInstruction[0x93] = InstructionUnknown;
	pAddressingMode[0x93] = AddressingModeNop;
	// STY - Zero Page,X
	CyclesNumberTable[0x94] = 0x04;
	pInstruction[0x94] = InstructionSty;
	pAddressingMode[0x94] = AddressingModeZeroPageX;
	// STA - Zero Page,X
	CyclesNumberTable[0x95] = 0x04;
	pInstruction[0x95] = InstructionSta;
	pAddressingMode[0x95] = AddressingModeZeroPageX;
	// STX - Zero Page,Y
	CyclesNumberTable[0x96] = 0x04;
	pInstruction[0x96] = InstructionStx;
	pAddressingMode[0x96] = AddressingModeZeroPageY;
	// Future Expansion
	CyclesNumberTable[0x97] = 0x02;
	pInstruction[0x97] = InstructionUnknown;
	pAddressingMode[0x97] = AddressingModeNop;
	// TYA
	CyclesNumberTable[0x98] = 0x02;
	pInstruction[0x98] = InstructionTya;
	pAddressingMode[0x98] = AddressingModeNop;
	// STA - Absolute,Y
	CyclesNumberTable[0x99] = 0x05;
	pInstruction[0x99] = InstructionSta;
	pAddressingMode[0x99] = AddressingModeAbsoluteY;
	// TXS
	CyclesNumberTable[0x9A] = 0x02;
	pInstruction[0x9A] = InstructionTxs;
	pAddressingMode[0x9A] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0x9B] = 0x02;
	pInstruction[0x9B] = InstructionUnknown;
	pAddressingMode[0x9B] = AddressingModeNop;
	// STZ - Absolute
	CyclesNumberTable[0x9C] = 0x04;
	pInstruction[0x9C] = InstructionStz;
	pAddressingMode[0x9C] = AddressingModeAbsolute;
	// STA - Absolute,X
	CyclesNumberTable[0x9D] = 0x05;
	pInstruction[0x9D] = InstructionSta;
	pAddressingMode[0x9D] = AddressingModeAbsoluteX;
	// STZ - Absolute,X
	CyclesNumberTable[0x9E] = 0x05;
	pInstruction[0x9E] = InstructionStz;
	pAddressingMode[0x9E] = AddressingModeAbsoluteX;
	// BBS1
	CyclesNumberTable[0x9F] = 0x02;
	pInstruction[0x9F] = InstructionBbs1;
	pAddressingMode[0x9F] = AddressingModeNone;
	// LDY - Immediate
	CyclesNumberTable[0xA0] = 0x03;
	pInstruction[0xA0] = InstructionLdy;
	pAddressingMode[0xA0] = AddressingModeImmediate;
	// LDA - (Indirect,X)
	CyclesNumberTable[0xA1] = 0x06;
	pInstruction[0xA1] = InstructionLda;
	pAddressingMode[0xA1] = AddressingModeIndirectX;
	// LDX - Immediate
	CyclesNumberTable[0xA2] = 0x03;
	pInstruction[0xA2] = InstructionLdx;
	pAddressingMode[0xA2] = AddressingModeImmediate;
	// Future Expansion
	CyclesNumberTable[0xA3] = 0x02;
	pInstruction[0xA3] = InstructionUnknown;
	pAddressingMode[0xA3] = AddressingModeNop;
	// LDY - Zero Page
	CyclesNumberTable[0xA4] = 0x03;
	pInstruction[0xA4] = InstructionLdy;
	pAddressingMode[0xA4] = AddressingModeZeroPage;
	// LDA - Zero Page
	CyclesNumberTable[0xA5] = 0x03;
	pInstruction[0xA5] = InstructionLda;
	pAddressingMode[0xA5] = AddressingModeZeroPage;
	// LDX - Zero Page
	CyclesNumberTable[0xA6] = 0x03;
	pInstruction[0xA6] = InstructionLdx;
	pAddressingMode[0xA6] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0xA7] = 0x02;
	pInstruction[0xA7] = InstructionUnknown;
	pAddressingMode[0xA7] = AddressingModeNop;
	// TAY
	CyclesNumberTable[0xA8] = 0x02;
	pInstruction[0xA8] = InstructionTay;
	pAddressingMode[0xA8] = AddressingModeNop;
	// LDA - Immediate
	CyclesNumberTable[0xA9] = 0x03;
	pInstruction[0xA9] = InstructionLda;
	pAddressingMode[0xA9] = AddressingModeImmediate;
	// TAX
	CyclesNumberTable[0xAA] = 0x02;
	pInstruction[0xAA] = InstructionTax;
	pAddressingMode[0xAA] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xAB] = 0x02;
	pInstruction[0xAB] = InstructionUnknown;
	pAddressingMode[0xAB] = AddressingModeNop;
	// LDY - Absolute
	CyclesNumberTable[0xAC] = 0x04;
	pInstruction[0xAC] = InstructionLdy;
	pAddressingMode[0xAC] = AddressingModeAbsolute;
	// LDA - Absolute
	CyclesNumberTable[0xAD] = 0x04;
	pInstruction[0xAD] = InstructionLda;
	pAddressingMode[0xAD] = AddressingModeAbsolute;
	// LDX - Absolute
	CyclesNumberTable[0xAE] = 0x04;
	pInstruction[0xAE] = InstructionLdx;
	pAddressingMode[0xAE] = AddressingModeAbsolute;
	// BBS2
	CyclesNumberTable[0xAF] = 0x02;
	pInstruction[0xAF] = InstructionBbs2;
	pAddressingMode[0xAF] = AddressingModeNone;
	// BCS
	CyclesNumberTable[0xB0] = 0x02;
	pInstruction[0xB0] = InstructionBcs;
	pAddressingMode[0xB0] = AddressingModeNone;
	// LDA - (Indirect),Y
	CyclesNumberTable[0xB1] = 0x05;
	pInstruction[0xB1] = InstructionLda;
	pAddressingMode[0xB1] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0xB2] = 0x03;
	pInstruction[0xB2] = InstructionUnknown;
	pAddressingMode[0xB2] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xB3] = 0x02;
	pInstruction[0xB3] = InstructionUnknown;
	pAddressingMode[0xB3] = AddressingModeNop;
	// LDY - Zero Page,X
	CyclesNumberTable[0xB4] = 0x04;
	pInstruction[0xB4] = InstructionLdy;
	pAddressingMode[0xB4] = AddressingModeZeroPageX;
	// LDA - Zero Page,X
	CyclesNumberTable[0xB5] = 0x04;
	pInstruction[0xB5] = InstructionLda;
	pAddressingMode[0xB5] = AddressingModeZeroPageX;
	// LDX - Zero Page,Y
	CyclesNumberTable[0xB6] = 0x04;
	pInstruction[0xB6] = InstructionLdx;
	pAddressingMode[0xB6] = AddressingModeZeroPageY;
	// Future Expansion
	CyclesNumberTable[0xB7] = 0x02;
	pInstruction[0xB7] = InstructionUnknown;
	pAddressingMode[0xB7] = AddressingModeNop;
	// CLVReadFlash
	CyclesNumberTable[0xB8] = 0x02;
	pInstruction[0xB8] = InstructionClv;
	pAddressingMode[0xB8] = AddressingModeNop;
	// LDA - Absolute,Y
	CyclesNumberTable[0xB9] = 0x04;
	pInstruction[0xB9] = InstructionLda;
	pAddressingMode[0xB9] = AddressingModeAbsoluteY;
	// TSX
	CyclesNumberTable[0xBA] = 0x02;
	pInstruction[0xBA] = InstructionTsx;
	pAddressingMode[0xBA] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xBB] = 0x02;
	pInstruction[0xBB] = InstructionUnknown;
	pAddressingMode[0xBB] = AddressingModeNop;
	// LDY - Absolute,X
	CyclesNumberTable[0xBC] = 0x04;
	pInstruction[0xBC] = InstructionLdy;
	pAddressingMode[0xBC] = AddressingModeAbsoluteX;
	// LDA - Absolute,X
	CyclesNumberTable[0xBD] = 0x04;
	pInstruction[0xBD] = InstructionLda;
	pAddressingMode[0xBD] = AddressingModeAbsoluteX;
	// LDX - Absolute,Y
	CyclesNumberTable[0xBE] = 0x04;
	pInstruction[0xBE] = InstructionLdx;
	pAddressingMode[0xBE] = AddressingModeAbsoluteY;
	// BBS3
	CyclesNumberTable[0xBF] = 0x02;
	pInstruction[0xBF] = InstructionBbs3;
	pAddressingMode[0xBF] = AddressingModeNone;
	// CPY - Immediate
	CyclesNumberTable[0xC0] = 0x03;
	pInstruction[0xC0] = InstructionCpy;
	pAddressingMode[0xC0] = AddressingModeImmediate;
	// CMP - (Indirect,X)
	CyclesNumberTable[0xC1] = 0x06;
	pInstruction[0xC1] = InstructionCmp;
	pAddressingMode[0xC1] = AddressingModeIndirectX;
	// Future Expansion
	CyclesNumberTable[0xC2] = 0x02;
	pInstruction[0xC2] = InstructionUnknown;
	pAddressingMode[0xC2] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xC3] = 0x02;
	pInstruction[0xC3] = InstructionUnknown;
	pAddressingMode[0xC3] = AddressingModeNop;
	// CPY - Zero Page
	CyclesNumberTable[0xC4] = 0x03;
	pInstruction[0xC4] = InstructionCpy;
	pAddressingMode[0xC4] = AddressingModeZeroPage;
	// CMP - Zero Page
	CyclesNumberTable[0xC5] = 0x03;
	pInstruction[0xC5] = InstructionCmp;
	pAddressingMode[0xC5] = AddressingModeZeroPage;
	// DEC - Zero Page
	CyclesNumberTable[0xC6] = 0x05;
	pInstruction[0xC6] = InstructionDec;
	pAddressingMode[0xC6] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0xC7] = 0x02;
	pInstruction[0xC7] = InstructionUnknown;
	pAddressingMode[0xC7] = AddressingModeNop;
	// INY
	CyclesNumberTable[0xC8] = 0x02;
	pInstruction[0xC8] = InstructionIny;
	pAddressingMode[0xC8] = AddressingModeNop;
	// CMP - Immediate
	CyclesNumberTable[0xC9] = 0x03;
	pInstruction[0xC9] = InstructionCmp;
	pAddressingMode[0xC9] = AddressingModeImmediate;
	// DEX
	CyclesNumberTable[0xCA] = 0x02;
	pInstruction[0xCA] = InstructionDex;
	pAddressingMode[0xCA] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xCB] = 0x02;
	pInstruction[0xCB] = InstructionUnknown;
	pAddressingMode[0xCB] = AddressingModeNop;
	// CPY - Absolute
	CyclesNumberTable[0xCC] = 0x04;
	pInstruction[0xCC] = InstructionCpy;
	pAddressingMode[0xCC] = AddressingModeAbsolute;
	// CMP - Absolute
	CyclesNumberTable[0xCD] = 0x04;
	pInstruction[0xCD] = InstructionCmp;
	pAddressingMode[0xCD] = AddressingModeAbsolute;
	// DEC - Absolute
	CyclesNumberTable[0xCE] = 0x06;
	pInstruction[0xCE] = InstructionDec;
	pAddressingMode[0xCE] = AddressingModeAbsolute;
	// BBS4
	CyclesNumberTable[0xCF] = 0x02;
	pInstruction[0xCF] = InstructionBbs4;
	pAddressingMode[0xCF] = AddressingModeNone;
	// BNE
	CyclesNumberTable[0xD0] = 0x02;
	pInstruction[0xD0] = InstructionBne;
	pAddressingMode[0xD0] = AddressingModeNone;
	// CMP - (Indirect),Y
	CyclesNumberTable[0xD1] = 0x05;
	pInstruction[0xD1] = InstructionCmp;
	pAddressingMode[0xD1] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0xD2] = 0x03;
	pInstruction[0xD2] = InstructionUnknown;
	pAddressingMode[0xD2] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xD3] = 0x02;
	pInstruction[0xD3] = InstructionUnknown;
	pAddressingMode[0xD3] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xD4] = 0x02;
	pInstruction[0xD4] = InstructionUnknown;
	pAddressingMode[0xD4] = AddressingModeNop;
	// CMP - Zero Page,X
	CyclesNumberTable[0xD5] = 0x04;
	pInstruction[0xD5] = InstructionCmp;
	pAddressingMode[0xD5] = AddressingModeZeroPageX;
	// DEC - Zero Page,X
	CyclesNumberTable[0xD6] = 0x06;
	pInstruction[0xD6] = InstructionDec;
	pAddressingMode[0xD6] = AddressingModeZeroPageX;
	// Future Expansion
	CyclesNumberTable[0xD7] = 0x02;
	pInstruction[0xD7] = InstructionUnknown;
	pAddressingMode[0xD7] = AddressingModeNop;
	// CLD
	CyclesNumberTable[0xD8] = 0x02;
	pInstruction[0xD8] = InstructionCld;
	pAddressingMode[0xD8] = AddressingModeNop;
	// CMP - Absolute,Y
	CyclesNumberTable[0xD9] = 0x04;
	pInstruction[0xD9] = InstructionCmp;
	pAddressingMode[0xD9] = AddressingModeAbsoluteY;
	// PHX
	CyclesNumberTable[0xDA] = 0x03;
	pInstruction[0xDA] = InstructionPhx;
	pAddressingMode[0xDA] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xDB] = 0x02;
	pInstruction[0xDB] = InstructionUnknown;
	pAddressingMode[0xDB] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xDC] = 0x02;
	pInstruction[0xDC] = InstructionUnknown;
	pAddressingMode[0xDC] = AddressingModeNop;
	// CMP - Absolute,X
	CyclesNumberTable[0xDD] = 0x04;
	pInstruction[0xDD] = InstructionCmp;
	pAddressingMode[0xDD] = AddressingModeAbsoluteX;
	// DEC - Absolute,X
	CyclesNumberTable[0xDE] = 0x07;
	pInstruction[0xDE] = InstructionDec;
	pAddressingMode[0xDE] = AddressingModeAbsoluteX;
	// BBS5
	CyclesNumberTable[0xDF] = 0x02;
	pInstruction[0xDF] = InstructionBbs5;
	pAddressingMode[0xDF] = AddressingModeNone;
	// CPX - Immediate
	CyclesNumberTable[0xE0] = 0x03;
	pInstruction[0xE0] = InstructionCpx;
	pAddressingMode[0xE0] = AddressingModeImmediate;
	// SBC - (Indirect,X)
	CyclesNumberTable[0xE1] = 0x06;
	pInstruction[0xE1] = InstructionSbc;
	pAddressingMode[0xE1] = AddressingModeIndirectX;
	// Future Expansion
	CyclesNumberTable[0xE2] = 0x02;
	pInstruction[0xE2] = InstructionUnknown;
	pAddressingMode[0xE2] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xE3] = 0x02;
	pInstruction[0xE3] = InstructionUnknown;
	pAddressingMode[0xE3] = AddressingModeNop;
	// CPX - Zero Page
	CyclesNumberTable[0xE4] = 0x03;
	pInstruction[0xE4] = InstructionCpx;
	pAddressingMode[0xE4] = AddressingModeZeroPage;
	// SBC - Zero Page
	CyclesNumberTable[0xE5] = 0x03;
	pInstruction[0xE5] = InstructionSbc;
	pAddressingMode[0xE5] = AddressingModeZeroPage;
	// INC - Zero Page
	CyclesNumberTable[0xE6] = 0x05;
	pInstruction[0xE6] = InstructionInc;
	pAddressingMode[0xE6] = AddressingModeZeroPage;
	// Future Expansion
	CyclesNumberTable[0xE7] = 0x02;
	pInstruction[0xE7] = InstructionUnknown;
	pAddressingMode[0xE7] = AddressingModeNop;
	// INX
	CyclesNumberTable[0xE8] = 0x02;
	pInstruction[0xE8] = InstructionInx;
	pAddressingMode[0xE8] = AddressingModeNop;
	// SBC - Immediate
	CyclesNumberTable[0xE9] = 0x03;
	pInstruction[0xE9] = InstructionSbc;
	pAddressingMode[0xE9] = AddressingModeImmediate;
	// NOP
	CyclesNumberTable[0xEA] = 0x02;
	pInstruction[0xEA] = InstructionNop;
	pAddressingMode[0xEA] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xEB] = 0x02;
	pInstruction[0xEB] = InstructionUnknown;
	pAddressingMode[0xEB] = AddressingModeNop;
	// CPX - Absolute
	CyclesNumberTable[0xEC] = 0x04;
	pInstruction[0xEC] = InstructionCpx;
	pAddressingMode[0xEC] = AddressingModeAbsolute;
	// SBC - Absolute
	CyclesNumberTable[0xED] = 0x04;
	pInstruction[0xED] = InstructionSbc;
	pAddressingMode[0xED] = AddressingModeAbsolute;
	// INC - Absolute
	CyclesNumberTable[0xEE] = 0x06;
	pInstruction[0xEE] = InstructionInc;
	pAddressingMode[0xEE] = AddressingModeAbsolute;
	// BBS6
	CyclesNumberTable[0xEF] = 0x02;
	pInstruction[0xEF] = InstructionBbs6;
	pAddressingMode[0xEF] = AddressingModeNone;
	// BEQ
	CyclesNumberTable[0xF0] = 0x02;
	pInstruction[0xF0] = InstructionBeq;
	pAddressingMode[0xF0] = AddressingModeNone;
	// SBC - (Indirect),Y
	CyclesNumberTable[0xF1] = 0x05;
	pInstruction[0xF1] = InstructionSbc;
	pAddressingMode[0xF1] = AddressingModeIndirectY;
	// Future Expansion
	CyclesNumberTable[0xF2] = 0x03;
	pInstruction[0xF2] = InstructionUnknown;
	pAddressingMode[0xF2] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xF3] = 0x02;
	pInstruction[0xF3] = InstructionUnknown;
	pAddressingMode[0xF3] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xF4] = 0x02;
	pInstruction[0xF4] = InstructionUnknown;
	pAddressingMode[0xF4] = AddressingModeNop;
	// SBC - Zero Page,X
	CyclesNumberTable[0xF5] = 0x04;
	pInstruction[0xF5] = InstructionSbc;
	pAddressingMode[0xF5] = AddressingModeZeroPageX;
	// INC - Zero Page,X
	CyclesNumberTable[0xF6] = 0x06;
	pInstruction[0xF6] = InstructionInc;
	pAddressingMode[0xF6] = AddressingModeZeroPageX;
	// Future Expansion
	CyclesNumberTable[0xF7] = 0x02;
	pInstruction[0xF7] = InstructionUnknown;
	pAddressingMode[0xF7] = AddressingModeNop;
	// SED
	CyclesNumberTable[0xF8] = 0x02;
	pInstruction[0xF8] = InstructionSed;
	pAddressingMode[0xF8] = AddressingModeNop;
	// SBC - Absolute,Y
	CyclesNumberTable[0xF9] = 0x04;
	pInstruction[0xF9] = InstructionSbc;
	pAddressingMode[0xF9] = AddressingModeAbsoluteY;
	// PLX
	CyclesNumberTable[0xFA] = 0x04;
	pInstruction[0xFA] = InstructionPlx;
	pAddressingMode[0xFA] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xFB] = 0x02;
	pInstruction[0xFB] = InstructionUnknown;
	pAddressingMode[0xFB] = AddressingModeNop;
	// Future Expansion
	CyclesNumberTable[0xFC] = 0x02;
	pInstruction[0xFC] = InstructionUnknown;
	pAddressingMode[0xFC] = AddressingModeNop;
	// SBC - Absolute,X
	CyclesNumberTable[0xFD] = 0x04;
	pInstruction[0xFD] = InstructionSbc;
	pAddressingMode[0xFD] = AddressingModeAbsoluteX;
	// INC - Absolute,X
	CyclesNumberTable[0xFE] = 0x07;
	pInstruction[0xFE] = InstructionInc;
	pAddressingMode[0xFE] = AddressingModeAbsoluteX;
	// BBS7
	CyclesNumberTable[0xFF] = 0x02;
	pInstruction[0xFF] = InstructionBbs7;
	pAddressingMode[0xFF] = AddressingModeNone;

	for (int i = 0; i < 0x300; i++)
	{
		ReadRegisterFns[i] = ReadRegister;
		WriteRegisterFns[i] = WriteRegister;
	}
	ReadRegisterFns[0x000] = ReadAddr1;
	ReadRegisterFns[0x001] = ReadAddr2;
	ReadRegisterFns[0x002] = ReadAddr3;
	ReadRegisterFns[0x003] = ReadAddr4;
	ReadRegisterFns[0x004] = ReadRegister;
	ReadRegisterFns[0x005] = ReadRegister;
	ReadRegisterFns[0x006] = ReadRegister;
	ReadRegisterFns[0x007] = ReadRegister;
	ReadRegisterFns[0x008] = ReadSBuf;
	ReadRegisterFns[0x009] = ReadRegister;
	ReadRegisterFns[0x00A] = ReadRegister;
	ReadRegisterFns[0x00B] = ReadRegister;
	ReadRegisterFns[0x00C] = ReadBkSel;
	ReadRegisterFns[0x00D] = ReadBkAdrL;
	ReadRegisterFns[0x00E] = ReadBkAdrH;
	ReadRegisterFns[0x00F] = ReadRegister;
	ReadRegisterFns[0x010] = ReadRegister;
	ReadRegisterFns[0x011] = ReadRegister;
	ReadRegisterFns[0x012] = ReadRegister;
	ReadRegisterFns[0x013] = ReadRegister;
	ReadRegisterFns[0x014] = ReadRegister;
	ReadRegisterFns[0x015] = ReadRegister;
	ReadRegisterFns[0x016] = ReadRegister;
	ReadRegisterFns[0x017] = ReadRegister;
	ReadRegisterFns[0x018] = ReadRegister;
	ReadRegisterFns[0x019] = ReadRegister;
	ReadRegisterFns[0x01A] = ReadRegister;
	ReadRegisterFns[0x01B] = ReadRegister;
	ReadRegisterFns[0x01C] = ReadRegister;
	ReadRegisterFns[0x01D] = ReadRegister;
	ReadRegisterFns[0x01E] = ReadRegister;
	ReadRegisterFns[0x01F] = ReadRegister;

	ReadRegisterFns[0x200] = ReadRegister;
	ReadRegisterFns[0x201] = ReadRegister;
	ReadRegisterFns[0x202] = ReadRegister;
	ReadRegisterFns[0x203] = ReadRegister;
	ReadRegisterFns[0x204] = ReadRegister;
	ReadRegisterFns[0x205] = ReadRegister;
	ReadRegisterFns[0x206] = ReadRegister;
	ReadRegisterFns[0x207] = ReadRegister;
	ReadRegisterFns[0x208] = ReadRegister;
	ReadRegisterFns[0x209] = ReadRegister;
	ReadRegisterFns[0x20A] = ReadRegister;
	ReadRegisterFns[0x20B] = ReadRegister;
	ReadRegisterFns[0x20C] = ReadRegister;
	ReadRegisterFns[0x20D] = ReadRegister;
	ReadRegisterFns[0x20E] = ReadRegister;
	ReadRegisterFns[0x20F] = ReadRegister;
	ReadRegisterFns[0x210] = ReadRegister;
	ReadRegisterFns[0x211] = ReadRegister;
	ReadRegisterFns[0x212] = ReadRegister;
	ReadRegisterFns[0x213] = ReadRegister;
	ReadRegisterFns[0x214] = ReadRegister;
	ReadRegisterFns[0x215] = ReadRegister;
	ReadRegisterFns[0x216] = ReadRegister;
	ReadRegisterFns[0x217] = ReadRegister;
	ReadRegisterFns[0x218] = ReadRegister;
	ReadRegisterFns[0x219] = ReadRegister;
	ReadRegisterFns[0x21A] = ReadRegister;
	ReadRegisterFns[0x21B] = ReadRegister;
	ReadRegisterFns[0x21C] = ReadRegister;
	ReadRegisterFns[0x21D] = ReadRegister;
	ReadRegisterFns[0x21E] = ReadRegister;
	ReadRegisterFns[0x21F] = ReadRegister;
	ReadRegisterFns[0x220] = ReadRegister;
	ReadRegisterFns[0x221] = ReadRegister;
	ReadRegisterFns[0x222] = ReadRegister;
	ReadRegisterFns[0x223] = ReadRegister;
	ReadRegisterFns[0x224] = ReadRegister;
	ReadRegisterFns[0x225] = ReadRegister;
	ReadRegisterFns[0x226] = ReadRegister;
	ReadRegisterFns[0x227] = ReadRegister;
	ReadRegisterFns[0x228] = ReadRegister;
	ReadRegisterFns[0x229] = ReadRegister;
	ReadRegisterFns[0x22A] = ReadRegister;
	ReadRegisterFns[0x22B] = ReadRegister;
	ReadRegisterFns[0x22C] = ReadRegister;
	ReadRegisterFns[0x22D] = ReadRegister;
	ReadRegisterFns[0x22E] = ReadRegister;
	ReadRegisterFns[0x22F] = ReadRegister;
	ReadRegisterFns[0x230] = ReadRegister;
	ReadRegisterFns[0x231] = ReadRegister;
	ReadRegisterFns[0x232] = ReadRegister;
	ReadRegisterFns[0x233] = ReadRegister;
	ReadRegisterFns[0x234] = ReadRegister;
	ReadRegisterFns[0x235] = ReadRegister;
	ReadRegisterFns[0x236] = ReadRegister;
	ReadRegisterFns[0x237] = ReadRegister;
	ReadRegisterFns[0x238] = ReadRegister;
	ReadRegisterFns[0x239] = ReadRegister;
	ReadRegisterFns[0x23A] = ReadRegister;
	ReadRegisterFns[0x23B] = ReadRegister;
	ReadRegisterFns[0x23C] = ReadRegister;
	ReadRegisterFns[0x23D] = ReadRegister;
	ReadRegisterFns[0x23E] = ReadRegister;
	ReadRegisterFns[0x23F] = ReadRegister;
	ReadRegisterFns[0x240] = ReadRegister;
	ReadRegisterFns[0x241] = ReadRegister;
	ReadRegisterFns[0x242] = ReadRegister;
	ReadRegisterFns[0x243] = ReadRegister;
	ReadRegisterFns[0x244] = ReadUrCon1;
	ReadRegisterFns[0x245] = ReadRegister;
	ReadRegisterFns[0x246] = ReadRegister;
	ReadRegisterFns[0x247] = ReadRegister;
	ReadRegisterFns[0x248] = ReadRegister;
	ReadRegisterFns[0x249] = ReadRegister;
	ReadRegisterFns[0x24A] = ReadRegister;
	ReadRegisterFns[0x24B] = ReadRegister;
	ReadRegisterFns[0x24C] = ReadRegister;
	ReadRegisterFns[0x24D] = ReadRegister;
	ReadRegisterFns[0x24E] = ReadRegister;
	ReadRegisterFns[0x24F] = ReadRegister;
	ReadRegisterFns[0x250] = ReadRegister;
	ReadRegisterFns[0x251] = ReadRegister;
	ReadRegisterFns[0x252] = ReadRegister;
	ReadRegisterFns[0x253] = ReadRegister;
	ReadRegisterFns[0x254] = ReadRegister;
	ReadRegisterFns[0x255] = ReadRegister;
	ReadRegisterFns[0x256] = ReadRegister;
	ReadRegisterFns[0x257] = ReadRegister;
	ReadRegisterFns[0x258] = ReadRegister;
	ReadRegisterFns[0x259] = ReadRegister;
	ReadRegisterFns[0x25A] = ReadRegister;
	ReadRegisterFns[0x25B] = ReadRegister;
	ReadRegisterFns[0x25C] = ReadRegister;
	ReadRegisterFns[0x25D] = ReadRegister;
	ReadRegisterFns[0x25E] = ReadRegister;
	ReadRegisterFns[0x25F] = ReadRegister;
	ReadRegisterFns[0x260] = ReadRegister;
	ReadRegisterFns[0x261] = ReadRegister;
	ReadRegisterFns[0x262] = ReadRegister;

	WriteRegisterFns[0x000] = WriteAddr1;
	WriteRegisterFns[0x001] = WriteAddr2;
	WriteRegisterFns[0x002] = WriteAddr3;
	WriteRegisterFns[0x003] = WriteAddr4;
	WriteRegisterFns[0x004] = WriteIsr;
	WriteRegisterFns[0x005] = WriteTIsr;
	WriteRegisterFns[0x006] = WriteRegister;
	WriteRegisterFns[0x007] = WriteRegister;
	WriteRegisterFns[0x008] = WriteSBuf;
	WriteRegisterFns[0x009] = WriteRegister;
	WriteRegisterFns[0x00A] = WriteRegister;
	WriteRegisterFns[0x00B] = WriteRegister;
	WriteRegisterFns[0x00C] = WriteBkSel;
	WriteRegisterFns[0x00D] = WriteBkAdrL;
	WriteRegisterFns[0x00E] = WriteBkAdrH;
	WriteRegisterFns[0x00F] = WriteRegister;
	WriteRegisterFns[0x010] = WriteRegister;
	WriteRegisterFns[0x011] = WriteRegister;
	WriteRegisterFns[0x012] = WriteRegister;
	WriteRegisterFns[0x013] = WriteRegister;
	WriteRegisterFns[0x014] = WriteRegister;
	WriteRegisterFns[0x015] = WriteRegister;
	WriteRegisterFns[0x016] = WriteRegister;
	WriteRegisterFns[0x017] = WriteRegister;
	WriteRegisterFns[0x018] = WriteRegister;
	WriteRegisterFns[0x019] = WriteRegister;
	WriteRegisterFns[0x01A] = WriteRegister;
	WriteRegisterFns[0x01B] = WriteRegister;
	WriteRegisterFns[0x01C] = WriteRegister;
	WriteRegisterFns[0x01D] = WriteRegister;
	WriteRegisterFns[0x01E] = WriteRegister;
	WriteRegisterFns[0x01F] = WriteRegister;

	WriteRegisterFns[0x200] = WriteSysCon;
	WriteRegisterFns[0x201] = WriteRegister;
	WriteRegisterFns[0x202] = WriteRegister;
	WriteRegisterFns[0x203] = WriteRegister;
	WriteRegisterFns[0x204] = WriteRegister;
	WriteRegisterFns[0x205] = WriteRegister;
	WriteRegisterFns[0x206] = WriteRegister;
	WriteRegisterFns[0x207] = WriteRegister;
	WriteRegisterFns[0x208] = WriteRegister;
	WriteRegisterFns[0x209] = WriteRegister;
	WriteRegisterFns[0x20A] = WriteRegister;
	WriteRegisterFns[0x20B] = WriteRegister;
	WriteRegisterFns[0x20C] = WriteRegister;
	WriteRegisterFns[0x20D] = WriteRegister;
	WriteRegisterFns[0x20E] = WriteRegister;
	WriteRegisterFns[0x20F] = WriteRegister;
	WriteRegisterFns[0x210] = WriteRegister;
	WriteRegisterFns[0x211] = WriteRegister;
	WriteRegisterFns[0x212] = WriteRegister;
	WriteRegisterFns[0x213] = WriteRegister;
	WriteRegisterFns[0x214] = WriteRegister;
	WriteRegisterFns[0x215] = WriteRegister;
	WriteRegisterFns[0x216] = WriteRegister;
	WriteRegisterFns[0x217] = WriteRegister;
	WriteRegisterFns[0x218] = WriteRegister;
	WriteRegisterFns[0x219] = WriteRegister;
	WriteRegisterFns[0x21A] = WriteRegister;
	WriteRegisterFns[0x21B] = WriteRegister;
	WriteRegisterFns[0x21C] = WriteRegister;
	WriteRegisterFns[0x21D] = WriteRegister;
	WriteRegisterFns[0x21E] = WriteRegister;
	WriteRegisterFns[0x21F] = WriteRegister;
	WriteRegisterFns[0x220] = WriteRegister;
	WriteRegisterFns[0x221] = WriteRegister;
	WriteRegisterFns[0x222] = WriteRegister;
	WriteRegisterFns[0x223] = WriteRegister;
	WriteRegisterFns[0x224] = WriteRegister;
	WriteRegisterFns[0x225] = WriteRegister;
	WriteRegisterFns[0x226] = WriteRegister;
	WriteRegisterFns[0x227] = WriteRegister;
	WriteRegisterFns[0x228] = WriteRegister;
	WriteRegisterFns[0x229] = WriteRegister;
	WriteRegisterFns[0x22A] = WriteRegister;
	WriteRegisterFns[0x22B] = WriteRegister;
	WriteRegisterFns[0x22C] = WriteRegister;
	WriteRegisterFns[0x22D] = WriteRegister;
	WriteRegisterFns[0x22E] = WriteRegister;
	WriteRegisterFns[0x22F] = WriteRegister;
	WriteRegisterFns[0x230] = WriteRegister;
	WriteRegisterFns[0x231] = WriteRegister;
	WriteRegisterFns[0x232] = WriteRegister;
	WriteRegisterFns[0x233] = WriteRegister;
	WriteRegisterFns[0x234] = WriteRegister;
	WriteRegisterFns[0x235] = WriteRegister;
	WriteRegisterFns[0x236] = WriteRegister;
	WriteRegisterFns[0x237] = WriteRegister;
	WriteRegisterFns[0x238] = WriteRegister;
	WriteRegisterFns[0x239] = WriteRegister;
	WriteRegisterFns[0x23A] = WriteRegister;
	WriteRegisterFns[0x23B] = WriteRegister;
	WriteRegisterFns[0x23C] = WriteRegister;
	WriteRegisterFns[0x23D] = WriteRegister;
	WriteRegisterFns[0x23E] = WriteRegister;
	WriteRegisterFns[0x23F] = WriteRegister;
	WriteRegisterFns[0x240] = WriteRegister;
	WriteRegisterFns[0x241] = WriteRegister;
	WriteRegisterFns[0x242] = WriteRegister;
	WriteRegisterFns[0x243] = WriteRegister;
	WriteRegisterFns[0x244] = WriteUsCon1;
	WriteRegisterFns[0x245] = WriteRegister;
	WriteRegisterFns[0x246] = WriteRegister;
	WriteRegisterFns[0x247] = WriteRegister;
	WriteRegisterFns[0x248] = WriteRegister;
	WriteRegisterFns[0x249] = WriteRegister;
	WriteRegisterFns[0x24A] = WriteRegister;
	WriteRegisterFns[0x24B] = WriteRegister;
	WriteRegisterFns[0x24C] = WriteRegister;
	WriteRegisterFns[0x24D] = WriteRegister;
	WriteRegisterFns[0x24E] = WriteRegister;
	WriteRegisterFns[0x24F] = WriteRegister;
	WriteRegisterFns[0x250] = WriteRegister;
	WriteRegisterFns[0x251] = WriteRegister;
	WriteRegisterFns[0x252] = WriteRegister;
	WriteRegisterFns[0x253] = WriteRegister;
	WriteRegisterFns[0x254] = WriteRegister;
	WriteRegisterFns[0x255] = WriteRegister;
	WriteRegisterFns[0x256] = WriteRegister;
	WriteRegisterFns[0x257] = WriteRegister;
	WriteRegisterFns[0x258] = WriteRegister;
	WriteRegisterFns[0x259] = WriteRegister;
	WriteRegisterFns[0x25A] = WriteRegister;
	WriteRegisterFns[0x25B] = WriteRegister;
	WriteRegisterFns[0x25C] = WriteRegister;
	WriteRegisterFns[0x25D] = WriteRegister;
	WriteRegisterFns[0x25E] = WriteRegister;
	WriteRegisterFns[0x25F] = WriteRegister;
	WriteRegisterFns[0x260] = WriteRegister;
	WriteRegisterFns[0x261] = WriteRegister;
	WriteRegisterFns[0x262] = WriteRegister;
}

// 设置寄存器默认状态
void McuRegesterInit()
{
	pRam[0x1B] |= 0x20;
	pRam[0x1B] |= 0x40;
	pRam[0x1B] |= 0x80;
	pRam[0x202] = 0x6C;
	pRam[0x203] |= 0x40;
	pRam[0x207] |= 0x01;
	pRam[0x207] |= 0x02;
	pRam[0x207] |= 0x04;
	pRam[0x207] |= 0x08;
	pRam[0x215] = 0xFF;
	pRam[0x219] = 0xFF;
	pRam[0x21C] = 0xFF;
	pRam[0x220] = 0x08;
	pRam[0x222] |= 0x04;
	pRam[0x222] |= 0x40;
	pRam[0x22E] |= 0x01;
	pRam[0x22E] &= 0xDF;
	pRam[0x24B] |= 0x01;
	pRam[0x24B] |= 0x04;
	pRam[0x25B] |= 0x20;
	pRam[0x25B] |= 0x40;
	pRam[0x25B] |= 0x80;
}

// 重置MCU状态
void McuReset()
{
	cpu.A = 0;
	cpu.X = 0;
	cpu.Y = 0;
	cpu.P = 0x20;
	cpu.S = 0x1FF;
	cpu.PC = 0x350;
	Timer1Counter = 0;
	Timer2Counter = 0;
	Timer3Counter = 0;
	Timer4Counter = 0;
}

// done
void McuStep()
{
#ifdef _DEBUG
	UINT32 address = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF);
	//if (cpu.PC > 0xFFFF)
	//{
	//	debug_save_regedits();
	//}
	//if (address == 0x00F25AA3)
	//{
	//	UINT32 a = *(UINT16*)(pRam + 0x28) + 0x31;
	//	segPageBaseAddrs[a >> 0x13][a & 0x7FFFF] = 0x00;
	//}
	if (address == 0x00E9EB20)
	{
		printf("%d", cpu.PC);
	}
	regedits[regedits_index].A = cpu.A;
	regedits[regedits_index].S = cpu.S;
	regedits[regedits_index].X = cpu.X;
	regedits[regedits_index].Y = cpu.Y;
	regedits[regedits_index].P = cpu.P;
	regedits[regedits_index].PC = cpu.PC;
	regedits[regedits_index].opcode = segPageBaseAddrs[address >> 0x13][address & 0x7FFFF];
	regedits[regedits_index].datas[0] = segPageBaseAddrs[(address + 1) >> 0x13][(address + 1) & 0x7FFFF];
	regedits[regedits_index].datas[1] = segPageBaseAddrs[(address + 2) >> 0x13][(address + 2) & 0x7FFFF];
	regedits[regedits_index].datas[2] = segPageBaseAddrs[(address + 3) >> 0x13][(address + 3) & 0x7FFFF];
	regedits[regedits_index].counter = CycleCounter;
	regedits_index++;
#endif
	UINT32 addr = BankAddressList[cpu.PC >> 0x0C] | (cpu.PC & 0x0FFF); // bank地址拼接pc地址
	McuOpCode = segPageBaseAddrs[addr >> 0x13][addr & 0x7FFFF]; // opcode 读取文件映射数据
	cpu.PC++;
	CycleCounter += CyclesNumberTable[McuOpCode];
	pAddressingMode[McuOpCode]();
	pInstruction[McuOpCode]();
	pInterruptHandle[bWantInt](); // _00413A90为1进中断
}

// 中断响应函数
void McuInterruptHandle()
{
	if (cpu.P & 0x04)
	{
		return; // 软中断
	}
	bWantInt = 0;
	UINT32 idx = 0xFF;
	// 推测以下有些中断有先后顺序，未弄清楚先后顺序前请不要轻易调整顺序，否则可能导致模拟器使用感觉卡顿
	if ((pRam[0x04] & 0x80) && (pRam[0x23A] & 0x80))
	{
		idx = 0x02; // PI_ISR_routine
	}
	else if ((pRam[0x04] & 0x01) && (pRam[0x23A] & 0x01))
	{
		idx = 0x13; // CT_ISR_routine
	}
	else if (pRam[0x09] & 0x40)
	{
		idx = 0x0C; // RXD_ISR_routine
	}
	else if ((pRam[0x04] & 0x02) && (pRam[0x23A] & 0x02))
	{
		idx = 0x12; // MMC_RSP_ISR_routine
	}
	else if ((pRam[0x05] & 0x01) && (pRam[0x23B] & 0x01))
	{
		idx = 0x03; // ST1_ISR_routine
	}
	else if ((pRam[0x05] & 0x02) && (pRam[0x23B] & 0x02))
	{
		idx = 0x04; // ST2_ISR_routine
	}
	else if ((pRam[0x05] & 0x04) && (pRam[0x23B] & 0x04))
	{
		idx = 0x05; // ST3_ISR_routine
	}
	else if ((pRam[0x05] & 0x08) && (pRam[0x23B] & 0x08))
	{
		idx = 0x06; // ST4_ISR_routine
	}
	if (idx == 0xFF)
	{
		return;
	}
	cpu.S -= 0x03;
	*(UINT32*)(pRam + cpu.S) = (cpu.PC << 16) | (cpu.P << 8);
	cpu.PC = idx * 4 + 0x300;
	cpu.P |= 0x04;
}

// MCU定时器模拟
void McuTimerImplement(UINT32 v1)
{
	if (pRam[0x226] & 0x01) // _STCON
	{
		Timer1Counter += v1;
		if (Timer1Counter >= 0x100)
		{
			if (pRam[0x23B] & 0x01) // _TIER
			{
				pRam[0x05] |= 0x01; // _TISR
				bWantInt = 0x01;
				SleepFlag = 0x00;
				pRam[0x200] &= 0xF7;
			}
			Timer1Counter = pRam[0x227]; // _ST1LD
		}
	}
	if (pRam[0x226] & 0x02)
	{
		Timer2Counter += v1;
		if (Timer2Counter >= 0x100)
		{
			if (pRam[0x23B] & 0x02)
			{
				pRam[0x05] |= 0x02;
				bWantInt = 0x01;
				SleepFlag = 0x00;
				pRam[0x200] &= 0xF7;
			}
			Timer2Counter = pRam[0x228]; // _ST2LD
		}
	}
	if (pRam[0x226] & 0x04)
	{
		Timer3Counter += v1;
		if (Timer3Counter >= 0x100)
		{
			if (pRam[0x23B] & 0x04)
			{
				pRam[0x05] |= 0x04;
				bWantInt = 0x01;
				SleepFlag = 0x00;
				pRam[0x200] &= 0xF7;
			}
			Timer3Counter = pRam[0x229]; // _ST3LD
		}
	}
	if (pRam[0x226] & 0x08)
	{
		Timer4Counter += v1;
		if (Timer4Counter >= 0x100)
		{
			if (pRam[0x23B] & 0x08)
			{
				pRam[0x05] |= 0x08;
				bWantInt = 0x01;
				SleepFlag = 0x00;
				pRam[0x200] &= 0xF7;
			}
			Timer4Counter = pRam[0x22A]; // _ST4LD
		}
	}
	if (pRam[0x22E] & 0x10) // _STCTCON
	{
		UniversalTimerCounter += v1;
		if (UniversalTimerCounter >= 0x1000)
		{
			if (pRam[0x23A] & 0x02) // _IER
			{
				pRam[0x04] |= 0x02;
				bWantInt = 0x01;
				SleepFlag = 0x00;
				pRam[0x200] &= 0xF7;
			}
			UniversalTimerCounter = pRam[0x22F]; // _CTLD
		}
	}
}

// done
void McuRtcImplement()
{
	if ((pRam[0x22E] & 0x40) == 0x00)
	{
		return;
	}
	if (pRam[0x234]++ == 59) // _RTCSEC
	{
		pRam[0x234] = 0;
		if (pRam[0x235]++ == 59) // _RTCMIN
		{
			pRam[0x235] = 0;
			if (pRam[0x236]++ == 23) // _RTCHR
			{
				pRam[0x236] = 0;
				if (pRam[0x237]++ == 0xFF) // _RTCDAYL
				{
					if (pRam[0x238]++ == 1) // _RTCDAYH
					{
						pRam[0x238] = 0;
					}
				}
			}
		}
	}
	if ((pRam[0x22E] & 0x20) == 0x00) // _STCTCON
	{
		return;
	}
	// 闹钟时间是否到
	if ((pRam[0x235] == pRam[0x230]) // _RTCMIN == _ALMMIN
		&& (pRam[0x236] == pRam[0x231]) // _RTCHR == _ALMHR
		&& (pRam[0x232] == pRam[0x232])
		&& (pRam[0x233] == pRam[0x233]))
	{
		pRam[0x04] |= 0x01; // _ISR
	}
}

// 读数据
void LoadOpData()
{
	visitFlashAddr = BankAddressList[visitAddr >> 0x0C] | (visitAddr & 0x0FFF);
	if (visitFlashAddr < 0x300)
	{
		ReadRegisterFns[visitFlashAddr]();
	}
	else
	{
		ReadOpData();
	}
}

// 写数据
void SaveOpData()
{
	visitFlashAddr = BankAddressList[visitAddr >> 0x0C] | (visitAddr & 0x0FFF);
	if (visitFlashAddr < 0x300)
	{
		WriteRegisterFns[visitFlashAddr]();
	}
	else
	{
		WriteOpData();
	}
}

#ifdef _DEBUG
const UINT32 addrModeOpDataCountTable[0x10] = { 0, 1, 2, 1, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0xFFFFFFFF, 0, 0x00403683 };
char opCodeStrs[61][8] = {
	"ADC  ", "AND  ", "ASL  ", "ASL  A", "BCC  ", "BCS  ", "BEQ  ", "BIT  ",
	"BMI  ", "BNE  ", "BPL  ", "BRK  ", "BVC  ", "BVS  ", "CLC  ", "CLD  ",
	"CLI  ", "CLV  ", "ASL  ", "CMP  ", "CPX  ", "CPY  ", "DEC  ", "DEX  ",
	"DEY  ", "EOR  ", "INC  ", "INX  ", "INY  ", "JMP  ", "JSR  ", "LDA  ",
	"LDX  ", "LDY  ", "LSR  ", "LSR  A", "NOP  ", "ORA  ", "PHA  ", "PHP  ",
	"PLA  ", "PLP  ", "ROL  ", "ROL  A", "ROR  ", "ROR  A", "RTI  ", "RTS  ",
	"SBC  ", "SEC  ", "SED  ", "SEI  ", "STA  ", "STX  ", "STY  ", "TAX  ",
	"TAY  ", "TSX  ", "TXA  ", "TXS  ", "TYA  " };
const CHAR callStackAddrModeFmts[12][30] = {
	"%s  %s        %s",
	"%s  %s %s     %s#$%s",
	"%s  %s %s %s  %s$%s%s",
	"%s  %s %s     %s$%s%s",
	"%s  %s %s %s  %s($%s%s)",
	"%s  %s %s %s  %s$%s%s,X",
	"%s  %s %s %s  %s$%s%s,Y",
	"%s  %s %s     %s$%s",
	"%s  %s %s     %s$%s,X",
	"%s  %s %s     %s$%s,Y",
	"%s  %s %s     %s($%s,X)",
	"%s  %s %s     %s($%s),Y",
};

void InitOpCodePrintStrs()
{
	strcpy(opCodePrintStrs[0x00], opCodeStrs[0]);
	strcpy(opCodePrintStrs[0x01], opCodeStrs[1]);
	strcpy(opCodePrintStrs[0x02], opCodeStrs[2]);
	strcpy(opCodePrintStrs[0x03], opCodeStrs[3]);
	strcpy(opCodePrintStrs[0x04], opCodeStrs[4]);
	strcpy(opCodePrintStrs[0x05], opCodeStrs[5]);
	strcpy(opCodePrintStrs[0x06], opCodeStrs[6]);
	strcpy(opCodePrintStrs[0x07], opCodeStrs[7]);
	strcpy(opCodePrintStrs[0x08], opCodeStrs[8]);
	strcpy(opCodePrintStrs[0x09], opCodeStrs[9]);
	strcpy(opCodePrintStrs[0x0A], opCodeStrs[10]);
	strcpy(opCodePrintStrs[0x0B], opCodeStrs[11]);
	strcpy(opCodePrintStrs[0x0C], opCodeStrs[12]);
	strcpy(opCodePrintStrs[0x0D], opCodeStrs[13]);
	strcpy(opCodePrintStrs[0x0E], opCodeStrs[14]);
	strcpy(opCodePrintStrs[0x0F], opCodeStrs[15]);
	strcpy(opCodePrintStrs[0x10], opCodeStrs[16]);
	strcpy(opCodePrintStrs[0x11], opCodeStrs[17]);
	strcpy(opCodePrintStrs[0x02], opCodeStrs[18]);
	strcpy(opCodePrintStrs[0x12], opCodeStrs[19]);
	strcpy(opCodePrintStrs[0x13], opCodeStrs[20]);
	strcpy(opCodePrintStrs[0x14], opCodeStrs[21]);
	strcpy(opCodePrintStrs[0x15], opCodeStrs[22]);
	strcpy(opCodePrintStrs[0x16], opCodeStrs[23]);
	strcpy(opCodePrintStrs[0x17], opCodeStrs[24]);
	strcpy(opCodePrintStrs[0x18], opCodeStrs[25]);
	strcpy(opCodePrintStrs[0x19], opCodeStrs[26]);
	strcpy(opCodePrintStrs[0x1A], opCodeStrs[27]);
	strcpy(opCodePrintStrs[0x1B], opCodeStrs[28]);
	strcpy(opCodePrintStrs[0x1C], opCodeStrs[29]);
	strcpy(opCodePrintStrs[0x1D], opCodeStrs[30]);
	strcpy(opCodePrintStrs[0x1E], opCodeStrs[31]);
	strcpy(opCodePrintStrs[0x1F], opCodeStrs[32]);
	strcpy(opCodePrintStrs[0x20], opCodeStrs[33]);
	strcpy(opCodePrintStrs[0x21], opCodeStrs[34]);
	strcpy(opCodePrintStrs[0x22], opCodeStrs[35]);
	strcpy(opCodePrintStrs[0x23], opCodeStrs[36]);
	strcpy(opCodePrintStrs[0x24], opCodeStrs[37]);
	strcpy(opCodePrintStrs[0x25], opCodeStrs[38]);
	strcpy(opCodePrintStrs[0x26], opCodeStrs[39]);
	strcpy(opCodePrintStrs[0x27], opCodeStrs[40]);
	strcpy(opCodePrintStrs[0x28], opCodeStrs[41]);
	strcpy(opCodePrintStrs[0x29], opCodeStrs[42]);
	strcpy(opCodePrintStrs[0x2A], opCodeStrs[43]);
	strcpy(opCodePrintStrs[0x2B], opCodeStrs[44]);
	strcpy(opCodePrintStrs[0x2C], opCodeStrs[45]);
	strcpy(opCodePrintStrs[0x2D], opCodeStrs[46]);
	strcpy(opCodePrintStrs[0x2E], opCodeStrs[47]);
	strcpy(opCodePrintStrs[0x2F], opCodeStrs[48]);
	strcpy(opCodePrintStrs[0x30], opCodeStrs[49]);
	strcpy(opCodePrintStrs[0x31], opCodeStrs[50]);
	strcpy(opCodePrintStrs[0x32], opCodeStrs[51]);
	strcpy(opCodePrintStrs[0x33], opCodeStrs[52]);
	strcpy(opCodePrintStrs[0x34], opCodeStrs[53]);
	strcpy(opCodePrintStrs[0x35], opCodeStrs[54]);
	strcpy(opCodePrintStrs[0x36], opCodeStrs[55]);
	strcpy(opCodePrintStrs[0x37], opCodeStrs[56]);
	strcpy(opCodePrintStrs[0x38], opCodeStrs[57]);
	strcpy(opCodePrintStrs[0x39], opCodeStrs[58]);
	strcpy(opCodePrintStrs[0x3A], opCodeStrs[59]);
	strcpy(opCodePrintStrs[0x3B], opCodeStrs[60]);
}

// done
UINT32 GetBankAddress(UINT32 v1)
{
	return BankAddressList[v1];
}

// done
UINT32 GetFlashAddress(UINT32 v1)
{
	return GetBankAddress(v1 >> 0x0C) | (v1 & 0x0FFF);
}

// done 无用
//UINT32 _00402FE9(UINT32 v1)
//{
//	return addrModeOpDataCountTable[opCodeAddrModeTable[segPageBaseAddrs[v1 >> 0x13][v1 & 0x7FFFF]]];
//}

// done v3=0 pCode=NULL
void DBG_Disassembly(UINT32* pPC, CHAR* buf, UINT32 v3, UINT32* pCode)
{
	CHAR addrHStr[12];
	UINT32 data;
	CHAR dataStr[12];
	UINT32 pcTmp;
	//UINT32 ebp_24;
	CHAR codeStr[12];
	UINT32 code;
	UINT32 pc = *pPC;
	UINT32 addrH;
	CHAR pcStr[12];
	CHAR addrLStr[12];
	UINT32 addrL;
	//ebp_24 = 8;

	if (v3 == 0)
	{
		*pPC = GetFlashAddress(*pPC);
		//ebp_24 = 4;
	}
	pcTmp = *pPC;
	if (pCode)
	{
		code = *pCode;
	}
	else
	{
		code = segPageBaseAddrs[*pPC >> 0x13][*pPC & 0x7FFFF];
	}
	*pPC += 1;
	sprintf(codeStr, "%02X", code);
	if (v3 == 1)
	{
		sprintf(pcStr, "%ul", pc);
	}
	else
	{
		sprintf(pcStr, "%04X", pc);
	}
	switch (addrModeOpDataCountTable[opCodeAddrModeTable[code]])
	{
	case 0:
		sprintf(buf, callStackAddrModeFmts[opCodeAddrModeTable[code]], pcStr, codeStr, opCodePrintStrs[opCodeToStrTable[code]]);
		break;
	case 1:
		data = segPageBaseAddrs[*pPC >> 0x13][*pPC & 0x7FFFF];
		*pPC += 1;
		if (opCodeAddrModeTable[code] == 3)
		{
			if (v3 == 1)
			{
				addrH = *pPC + data;
			}
			else
			{
				addrH = *pPC - pcTmp + pc + data;
			}
			if (data & 0x80)
			{
				addrH -= 0x100;
			}
			addrL = addrH & 0xFF;
			sprintf(addrLStr, "%02X", addrL);
			addrH >>= 8;
			sprintf(addrHStr, "%02X", addrH);
			sprintf(dataStr, "%02X", data);
			sprintf(buf, callStackAddrModeFmts[opCodeAddrModeTable[code]], pcStr, codeStr, dataStr, opCodePrintStrs[opCodeToStrTable[code]], addrHStr, addrLStr);
		}
		else
		{
			sprintf(addrLStr, "%02X", data);
			sprintf(buf, callStackAddrModeFmts[opCodeAddrModeTable[code]], pcStr, codeStr, addrLStr, opCodePrintStrs[opCodeToStrTable[code]], addrLStr);
		}
		break;
	case 2:
		addrL = segPageBaseAddrs[*pPC >> 0x13][*pPC & 0x7FFFF];
		*pPC += 1;
		sprintf(addrLStr, "%02X", addrL);
		addrH = segPageBaseAddrs[*pPC >> 0x13][*pPC & 0x7FFFF];
		*pPC += 1;
		sprintf(addrHStr, "%02X", addrH);
		sprintf(buf, callStackAddrModeFmts[opCodeAddrModeTable[code]], pcStr, codeStr, addrLStr, addrHStr, opCodePrintStrs[opCodeToStrTable[code]], addrHStr, addrLStr);
		break;
	}
	if (v3 == 0)
	{
		*pPC = *pPC - pcTmp + pc;
	}
}

void debug_save_regedits()
{
	char buffer[0x100];
	FILE* fp = fopen("regedits.log", "w");
	for (int i = 0; i < 0x100; i++)
	{
		struct _REGEDIT regedit = regedits[(UINT8)(regedits_index + i)];
		sprintf(buffer, "A:%02X S:%04X X:%02X Y:%02X P:%02X\n[%02X %02X %02X %02X] ",
			regedit.A, regedit.S, regedit.X, regedit.Y, regedit.P,
			regedit.opcode, regedit.datas[0], regedit.datas[1], regedit.datas[2]);
		UINT32 pc = regedit.PC;
		DBG_Disassembly(&pc, buffer + strlen(buffer), 0, NULL);
		fwrite(buffer, 1, strlen(buffer), fp);
		fwrite("\n", 1, 1, fp);
	}
	fclose(fp);
}
#endif
