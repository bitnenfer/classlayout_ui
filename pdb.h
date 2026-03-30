#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unordered_map>

#include "bodoq_engine/debug.h"
#include "bodoq_engine/string.h"
#include "bodoq_engine/array.h"
#include "bodoq_engine/hash_table.h"

//#define NOT_IMPLEMENTED_LEAF_TYPE(Type) DBG_LOG(Type " Not Implemented")
#define NOT_IMPLEMENTED_LEAF_TYPE(Type) 
#define ALERT DBG_PANIC
#define LOG PDB_LOG
#define ASSERT DBG_ASSERT
#define ALWAYS_ASSERT DBG_ASSERT
#define TempFmtByteBuffer(fmt, ...) *str::format("" fmt "", ##__VA_ARGS__)

#define MACHINE_TYPE_UNKNOWN		0x0
#define MACHINE_TYPE_ALPHA			0x184
#define MACHINE_TYPE_ALPHA64		0x284
#define MACHINE_TYPE_AM33			0x1d3
#define MACHINE_TYPE_AMD64			0x8664
#define MACHINE_TYPE_ARM			0x1c0
#define MACHINE_TYPE_ARM64			0xaa64
#define MACHINE_TYPE_ARMNT			0x1c4
#define MACHINE_TYPE_AXP64			0x284
#define MACHINE_TYPE_EBC			0xebc
#define MACHINE_TYPE_I386			0x14c
#define MACHINE_TYPE_IA64			0x200
#define MACHINE_TYPE_LOONGARCH32	0x6232
#define MACHINE_TYPE_LOONGARCH64	0x6264
#define MACHINE_TYPE_M32R			0x9041
#define MACHINE_TYPE_MIPS16			0x266
#define MACHINE_TYPE_MIPSFPU		0x366
#define MACHINE_TYPE_MIPSFPU16		0x466
#define MACHINE_TYPE_POWERPC		0x1f0
#define MACHINE_TYPE_POWERPCFP		0x1f1
#define MACHINE_TYPE_R4000			0x166
#define MACHINE_TYPE_RISCV32		0x5032
#define MACHINE_TYPE_RISCV64		0x5064
#define MACHINE_TYPE_RISCV128		0x5128
#define MACHINE_TYPE_SH3			0x1a2
#define MACHINE_TYPE_SH3DSP			0x1a3
#define MACHINE_TYPE_SH4			0x1a6
#define MACHINE_TYPE_SH5			0x1a8
#define MACHINE_TYPE_THUMB			0x1c2
#define MACHINE_TYPE_WCEMIPSV2		0x169

namespace pdb
{
	template<typename T>
	T* mallocType(size_t count)
	{
		if (void* ptr = malloc(sizeof(T) * count))
		{
			memset(ptr, 0, sizeof(T) * count);
			return (T*)ptr;
		}
		return nullptr;
	}

	inline void bufferReadBuffer(const uint8_t* pInputData, void* pWriteBuffer, uint32_t size)
	{
		memcpy(pWriteBuffer, pInputData, size);
	}

	template<class T>
	inline T bufferRead(const uint8_t* pInputData)
	{
		T value;
		bufferReadBuffer(pInputData, &value, sizeof(T));
		return value;
	}

	inline uint8_t bufferReadU8(const uint8_t* pInputData)
	{
		return *pInputData;
	}

	inline uint16_t bufferReadU16(const uint8_t* pInputData)
	{
		uint8_t b0 = pInputData[0];
		uint8_t b1 = pInputData[1];
		return (uint16_t)((b1 << 8) | b0);
	}

	inline uint32_t bufferReadU32(const uint8_t* pInputData)
	{
		uint8_t b0 = pInputData[0];
		uint8_t b1 = pInputData[1];
		uint8_t b2 = pInputData[2];
		uint8_t b3 = pInputData[3];
		return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
	}

	inline int8_t bufferReadS8(const uint8_t* pInputData)
	{
		uint8_t value = bufferReadU8(pInputData);
		return *((int8_t*)&value);
	}

	inline int16_t bufferReadS16(const uint8_t* pInputData)
	{
		uint16_t value = bufferReadU16(pInputData);
		return *((int16_t*)&value);
	}

	inline int32_t bufferReadS32(const uint8_t* pInputData)
	{
		uint32_t value = bufferReadU32(pInputData);
		return *((int32_t*)&value);
	}

	inline uint8_t bufferReadAndOffsetU8(const uint8_t* pInputData, uint64_t& offset)
	{
		uint8_t value = bufferReadU8(pInputData + offset);
		offset += 1;
		return value;
	}

	inline uint16_t bufferReadAndOffsetU16(const uint8_t* pInputData, uint64_t& offset)
	{
		uint16_t value = bufferReadU16(pInputData + offset);
		offset += 2;
		return value;
	}

	inline uint32_t bufferReadAndOffsetU32(const uint8_t* pInputData, uint64_t& offset)
	{
		uint32_t value = bufferReadU32(pInputData + offset);
		offset += 4;
		return value;
	}

	inline int8_t bufferReadAndOffsetS8(const uint8_t* pInputData, uint64_t& offset)
	{
		int8_t value = bufferReadS8(pInputData + offset);
		offset += 1;
		return value;
	}

	inline int16_t bufferReadAndOffsetS16(const uint8_t* pInputData, uint64_t& offset)
	{
		int16_t value = bufferReadS16(pInputData + offset);
		offset += 2;
		return value;
	}

	inline int32_t bufferReadAndOffsetS32(const uint8_t* pInputData, uint64_t& offset)
	{
		int32_t value = bufferReadS32(pInputData + offset);
		offset += 4;
		return value;
	}
	enum class EPDBStreamVersion : uint32_t
	{
		VC2 = 19941610,
		VC4 = 19950623,
		VC41 = 19950814,
		VC50 = 19960307,
		VC98 = 19970604,
		VC70Dep = 19990604,
		VC70 = 20000404,
		VC80 = 20030901,
		VC110 = 20091201,
		VC140 = 20140508,
	};

	struct PDBStreamHeader
	{
		EPDBStreamVersion version;
		uint32_t signature;
		uint32_t age;
		uint8_t guid[16];
	};

	struct PDBMergedBlocks
	{
		uint64_t blockSizeInBytes;
		const uint8_t* pBuffer;
		uint32_t blockCount;
	};

	struct IStreamReader
	{
		virtual const uint8_t* getBuffer() = 0;
		virtual uint64_t& getReadOffset() = 0;

		virtual const uint8_t* getBuffer(uint64_t offset) { return getBuffer() + offset; }
		virtual const uint8_t* getBufferCurr() { return getBuffer(getReadOffset()); }

		template<class T>
		T read(bool doOffset = true)
		{
			T value;
			readBuffer(&value, sizeof(T), doOffset);
			return value;
		}

		uint8_t readU8(bool doOffset = true)
		{
			return doOffset ? bufferReadAndOffsetU8(getBuffer(), getReadOffset()) : bufferReadU8(getBuffer(getReadOffset()));
		}

		uint16_t readU16(bool doOffset = true)
		{
			return doOffset ? bufferReadAndOffsetU16(getBuffer(), getReadOffset()) : bufferReadU16(getBuffer(getReadOffset()));
		}

		uint32_t readU32(bool doOffset = true)
		{
			return doOffset ? bufferReadAndOffsetU32(getBuffer(), getReadOffset()) : bufferReadU32(getBuffer(getReadOffset()));
		}

		uint8_t readS8(bool doOffset = true)
		{
			return doOffset ? bufferReadAndOffsetS8(getBuffer(), getReadOffset()) : bufferReadS8(getBuffer(getReadOffset()));
		}

		uint16_t readS16(bool doOffset = true)
		{
			return doOffset ? bufferReadAndOffsetS16(getBuffer(), getReadOffset()) : bufferReadS16(getBuffer(getReadOffset()));
		}

		uint32_t readS32(bool doOffset = true)
		{
			return doOffset ? bufferReadAndOffsetS32(getBuffer(), getReadOffset()) : bufferReadS32(getBuffer(getReadOffset()));
		}

		void readBuffer(void* pWriteBuffer, uint32_t size, bool doOffset = true)
		{
			memcpy(pWriteBuffer, getBuffer(getReadOffset()), size);
			if (doOffset) getReadOffset() += size;
		}

		void reset() { getReadOffset() = 0; }
	};


	struct PDBStreamBlock : public IStreamReader
	{
		PDBStreamBlock(PDBMergedBlocks blocks) :
			blocks(blocks),
			readOffset(0)
		{
		}

		PDBMergedBlocks& getBlocks() { return blocks; }

		virtual const uint8_t* getBuffer() override { return blocks.pBuffer; }
		virtual uint64_t& getReadOffset() override { return readOffset; }

	private:
		PDBMergedBlocks blocks;
		uint64_t readOffset;
	};

	static const char MSF7Magic[] =
	{
		0x4d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66,
		0x74, 0x20, 0x43, 0x2f, 0x43, 0x2b, 0x2b, 0x20,
		0x4d, 0x53, 0x46, 0x20, 0x37, 0x2e, 0x30, 0x30,
		0x0d, 0x0a, 0x1a, 0x44, 0x53, 0x00, 0x00, 0x00
	};

	struct MSFSuperBlock
	{
		char fileMagic[sizeof(MSF7Magic)];
		uint32_t blockSize;
		uint32_t freeBlockMapBlock;
		uint32_t numBlocks;
		uint32_t numDirectoryBytes;
		uint32_t unknown;
		uint32_t blockMapAddr;
	};

	struct MSFStreamBlock
	{
		uint32_t blockCount;
		uint32_t* pBlocks;
	};

	struct MSFStreamDirectory
	{
		uint32_t numStreams;
		uint32_t* pStreamSizes;
		MSFStreamBlock* pStreamBlocks;
	};

	enum class ELeafType : uint16_t
	{
		LEAF_TYPE_UNDEFINED = 0,
		LEAF_TYPE_POINTER = 0x1002,
		LEAF_TYPE_MODIFIER = 0x1001,
		LEAF_TYPE_PROCEDURE = 0x1008,
		LEAF_TYPE_MFUNCTION = 0x1009,
		LEAF_TYPE_LABEL = 0x000e,
		LEAF_TYPE_ARGLIST = 0x1201,
		LEAF_TYPE_FIELDLIST = 0x1203,
		LEAF_TYPE_ARRAY = 0x1503,
		LEAF_TYPE_CLASS = 0x1504,
		LEAF_TYPE_STRUCTURE = 0x1505,
		LEAF_TYPE_INTERFACE = 0x1519,
		LEAF_TYPE_UNION = 0x1506,
		LEAF_TYPE_ENUM = 0x1507,
		LEAF_TYPE_TYPESERVER2 = 0x1515,
		LEAF_TYPE_VFTABLE = 0x151d,
		LEAF_TYPE_VTSHAPE = 0x000a,
		LEAF_TYPE_BITFIELD = 0x1205,
		LEAF_TYPE_METHODLIST = 0x1206,
		LEAF_TYPE_PRECOMP = 0x1509,
		LEAF_TYPE_ENDPRECOMP = 0x0014,
		LEAF_TYPE_FUNC_ID = 0x1601,
		LEAF_TYPE_MFUNC_ID = 0x1602,
		LEAF_TYPE_BUILDINFO = 0x1603,
		LEAF_TYPE_SUBSTR_LIST = 0x1604,
		LEAF_TYPE_STRING_ID = 0x1605,
		LEAF_TYPE_UDT_SRC_LINE = 0x1606,
		LEAF_TYPE_UDT_MOD_SRC_LINE = 0x1607,
		LEAF_TYPE_BCLASS = 0x1400,
		LEAF_TYPE_BINTERFACE = 0x151a,
		LEAF_TYPE_VBCLASS = 0x1401,
		LEAF_TYPE_IVBCLASS = 0x1402,
		LEAF_TYPE_VFUNCTAB = 0x1409,
		LEAF_TYPE_STMEMBER = 0x150e,
		LEAF_TYPE_METHOD = 0x150f,
		LEAF_TYPE_MEMBER = 0x150d,
		LEAF_TYPE_NESTTYPE = 0x1510,
		LEAF_TYPE_ONEMETHOD = 0x1511,
		LEAF_TYPE_ENUMERATE = 0x1502,
		LEAF_TYPE_INDEX = 0x1404
	};

	enum class EPrimitiveBasicType
	{
		TYPE_NOTYPE = 0x0000,
		TYPE_ABS = 0x0001,
		TYPE_SEGMENT = 0x0002,
		TYPE_VOID = 0x0003,
		TYPE_HRESULT = 0x0008,
		TYPE_32PHRESULT = 0x0408,
		TYPE_64PHRESULT = 0x0608,
		TYPE_PVOID = 0x0103,
		TYPE_PFVOID = 0x0203,
		TYPE_PHVOID = 0x0303,
		TYPE_32PVOID = 0x0403,
		TYPE_32PFVOID = 0x0503,
		TYPE_64PVOID = 0x0603,
		TYPE_CURRENCY = 0x0004,
		TYPE_NBASICSTR = 0x0005,
		TYPE_FBASICSTR = 0x0006,
		TYPE_NOTTRANS = 0x0007,
		TYPE_BIT = 0x0060,
		TYPE_PASCHAR = 0x0061,
		TYPE_BOOL32FF = 0x0062,
		TYPE_CHAR = 0x0010,
		TYPE_PCHAR = 0x0110,
		TYPE_PFCHAR = 0x0210,
		TYPE_PHCHAR = 0x0310,
		TYPE_32PCHAR = 0x0410,
		TYPE_32PFCHAR = 0x0510,
		TYPE_64PCHAR = 0x0610,
		TYPE_UCHAR = 0x0020,
		TYPE_PUCHAR = 0x0120,
		TYPE_PFUCHAR = 0x0220,
		TYPE_PHUCHAR = 0x0320,
		TYPE_32PUCHAR = 0x0420,
		TYPE_32PFUCHAR = 0x0520,
		TYPE_64PUCHAR = 0x0620,
		TYPE_RCHAR = 0x0070,
		TYPE_PRCHAR = 0x0170,
		TYPE_PFRCHAR = 0x0270,
		TYPE_PHRCHAR = 0x0370,
		TYPE_32PRCHAR = 0x0470,
		TYPE_32PFRCHAR = 0x0570,
		TYPE_64PRCHAR = 0x0670,
		TYPE_WCHAR = 0x0071,
		TYPE_PWCHAR = 0x0171,
		TYPE_PFWCHAR = 0x0271,
		TYPE_PHWCHAR = 0x0371,
		TYPE_32PWCHAR = 0x0471,
		TYPE_32PFWCHAR = 0x0571,
		TYPE_64PWCHAR = 0x0671,
		TYPE_CHAR16 = 0x007a,
		TYPE_PCHAR16 = 0x017a,
		TYPE_PFCHAR16 = 0x027a,
		TYPE_PHCHAR16 = 0x037a,
		TYPE_32PCHAR16 = 0x047a,
		TYPE_32PFCHAR16 = 0x057a,
		TYPE_64PCHAR16 = 0x067a,
		TYPE_CHAR32 = 0x007b,
		TYPE_PCHAR32 = 0x017b,
		TYPE_PFCHAR32 = 0x027b,
		TYPE_PHCHAR32 = 0x037b,
		TYPE_32PCHAR32 = 0x047b,
		TYPE_32PFCHAR32 = 0x057b,
		TYPE_64PCHAR32 = 0x067b,
		TYPE_INT1 = 0x0068,
		TYPE_PINT1 = 0x0168,
		TYPE_PFINT1 = 0x0268,
		TYPE_PHINT1 = 0x0368,
		TYPE_32PINT1 = 0x0468,
		TYPE_32PFINT1 = 0x0568,
		TYPE_64PINT1 = 0x0668,
		TYPE_UINT1 = 0x0069,
		TYPE_PUINT1 = 0x0169,
		TYPE_PFUINT1 = 0x0269,
		TYPE_PHUINT1 = 0x0369,
		TYPE_32PUINT1 = 0x0469,
		TYPE_32PFUINT1 = 0x0569,
		TYPE_64PUINT1 = 0x0669,
		TYPE_SHORT = 0x0011,
		TYPE_PSHORT = 0x0111,
		TYPE_PFSHORT = 0x0211,
		TYPE_PHSHORT = 0x0311,
		TYPE_32PSHORT = 0x0411,
		TYPE_32PFSHORT = 0x0511,
		TYPE_64PSHORT = 0x0611,
		TYPE_USHORT = 0x0021,
		TYPE_PUSHORT = 0x0121,
		TYPE_PFUSHORT = 0x0221,
		TYPE_PHUSHORT = 0x0321,
		TYPE_32PUSHORT = 0x0421,
		TYPE_32PFUSHORT = 0x0521,
		TYPE_64PUSHORT = 0x0621,
		TYPE_INT2 = 0x0072,
		TYPE_PINT2 = 0x0172,
		TYPE_PFINT2 = 0x0272,
		TYPE_PHINT2 = 0x0372,
		TYPE_32PINT2 = 0x0472,
		TYPE_32PFINT2 = 0x0572,
		TYPE_64PINT2 = 0x0672,
		TYPE_UINT2 = 0x0073,
		TYPE_PUINT2 = 0x0173,
		TYPE_PFUINT2 = 0x0273,
		TYPE_PHUINT2 = 0x0373,
		TYPE_32PUINT2 = 0x0473,
		TYPE_32PFUINT2 = 0x0573,
		TYPE_64PUINT2 = 0x0673,
		TYPE_LONG = 0x0012,
		TYPE_ULONG = 0x0022,
		TYPE_PLONG = 0x0112,
		TYPE_PULONG = 0x0122,
		TYPE_PFLONG = 0x0212,
		TYPE_PFULONG = 0x0222,
		TYPE_PHLONG = 0x0312,
		TYPE_PHULONG = 0x0322,
		TYPE_32PLONG = 0x0412,
		TYPE_32PULONG = 0x0422,
		TYPE_32PFLONG = 0x0512,
		TYPE_32PFULONG = 0x0522,
		TYPE_64PLONG = 0x0612,
		TYPE_64PULONG = 0x0622,
		TYPE_INT4 = 0x0074,
		TYPE_PINT4 = 0x0174,
		TYPE_PFINT4 = 0x0274,
		TYPE_PHINT4 = 0x0374,
		TYPE_32PINT4 = 0x0474,
		TYPE_32PFINT4 = 0x0574,
		TYPE_64PINT4 = 0x0674,
		TYPE_UINT4 = 0x0075,
		TYPE_PUINT4 = 0x0175,
		TYPE_PFUINT4 = 0x0275,
		TYPE_PHUINT4 = 0x0375,
		TYPE_32PUINT4 = 0x0475,
		TYPE_32PFUINT4 = 0x0575,
		TYPE_64PUINT4 = 0x0675,
		TYPE_QUAD = 0x0013,
		TYPE_PQUAD = 0x0113,
		TYPE_PFQUAD = 0x0213,
		TYPE_PHQUAD = 0x0313,
		TYPE_32PQUAD = 0x0413,
		TYPE_32PFQUAD = 0x0513,
		TYPE_64PQUAD = 0x0613,
		TYPE_UQUAD = 0x0023,
		TYPE_PUQUAD = 0x0123,
		TYPE_PFUQUAD = 0x0223,
		TYPE_PHUQUAD = 0x0323,
		TYPE_32PUQUAD = 0x0423,
		TYPE_32PFUQUAD = 0x0523,
		TYPE_64PUQUAD = 0x0623,
		TYPE_INT8 = 0x0076,
		TYPE_PINT8 = 0x0176,
		TYPE_PFINT8 = 0x0276,
		TYPE_PHINT8 = 0x0376,
		TYPE_32PINT8 = 0x0476,
		TYPE_32PFINT8 = 0x0576,
		TYPE_64PINT8 = 0x0676,
		TYPE_UINT8 = 0x0077,
		TYPE_PUINT8 = 0x0177,
		TYPE_PFUINT8 = 0x0277,
		TYPE_PHUINT8 = 0x0377,
		TYPE_32PUINT8 = 0x0477,
		TYPE_32PFUINT8 = 0x0577,
		TYPE_64PUINT8 = 0x0677,
		TYPE_OCT = 0x0014,
		TYPE_POCT = 0x0114,
		TYPE_PFOCT = 0x0214,
		TYPE_PHOCT = 0x0314,
		TYPE_32POCT = 0x0414,
		TYPE_32PFOCT = 0x0514,
		TYPE_64POCT = 0x0614,
		TYPE_UOCT = 0x0024,
		TYPE_PUOCT = 0x0124,
		TYPE_PFUOCT = 0x0224,
		TYPE_PHUOCT = 0x0324,
		TYPE_32PUOCT = 0x0424,
		TYPE_32PFUOCT = 0x0524,
		TYPE_64PUOCT = 0x0624,
		TYPE_INT16 = 0x0078,
		TYPE_PINT16 = 0x0178,
		TYPE_PFINT16 = 0x0278,
		TYPE_PHINT16 = 0x0378,
		TYPE_32PINT16 = 0x0478,
		TYPE_32PFINT16 = 0x0578,
		TYPE_64PINT16 = 0x0678,
		TYPE_UINT16 = 0x0079,
		TYPE_PUINT16 = 0x0179,
		TYPE_PFUINT16 = 0x0279,
		TYPE_PHUINT16 = 0x0379,
		TYPE_32PUINT16 = 0x0479,
		TYPE_32PFUINT16 = 0x0579,
		TYPE_64PUINT16 = 0x0679,
		TYPE_REAL16 = 0x0046,
		TYPE_PREAL16 = 0x0146,
		TYPE_PFREAL16 = 0x0246,
		TYPE_PHREAL16 = 0x0346,
		TYPE_32PREAL16 = 0x0446,
		TYPE_32PFREAL16 = 0x0546,
		TYPE_64PREAL16 = 0x0646,
		TYPE_REAL32 = 0x0040,
		TYPE_PREAL32 = 0x0140,
		TYPE_PFREAL32 = 0x0240,
		TYPE_PHREAL32 = 0x0340,
		TYPE_32PREAL32 = 0x0440,
		TYPE_32PFREAL32 = 0x0540,
		TYPE_64PREAL32 = 0x0640,
		TYPE_REAL32PP = 0x0045,
		TYPE_PREAL32PP = 0x0145,
		TYPE_PFREAL32PP = 0x0245,
		TYPE_PHREAL32PP = 0x0345,
		TYPE_32PREAL32PP = 0x0445,
		TYPE_32PFREAL32PP = 0x0545,
		TYPE_64PREAL32PP = 0x0645,
		TYPE_REAL48 = 0x0044,
		TYPE_PREAL48 = 0x0144,
		TYPE_PFREAL48 = 0x0244,
		TYPE_PHREAL48 = 0x0344,
		TYPE_32PREAL48 = 0x0444,
		TYPE_32PFREAL48 = 0x0544,
		TYPE_64PREAL48 = 0x0644,
		TYPE_REAL64 = 0x0041,
		TYPE_PREAL64 = 0x0141,
		TYPE_PFREAL64 = 0x0241,
		TYPE_PHREAL64 = 0x0341,
		TYPE_32PREAL64 = 0x0441,
		TYPE_32PFREAL64 = 0x0541,
		TYPE_64PREAL64 = 0x0641,
		TYPE_REAL80 = 0x0042,
		TYPE_PREAL80 = 0x0142,
		TYPE_PFREAL80 = 0x0242,
		TYPE_PHREAL80 = 0x0342,
		TYPE_32PREAL80 = 0x0442,
		TYPE_32PFREAL80 = 0x0542,
		TYPE_64PREAL80 = 0x0642,
		TYPE_REAL128 = 0x0043,
		TYPE_PREAL128 = 0x0143,
		TYPE_PFREAL128 = 0x0243,
		TYPE_PHREAL128 = 0x0343,
		TYPE_32PREAL128 = 0x0443,
		TYPE_32PFREAL128 = 0x0543,
		TYPE_64PREAL128 = 0x0643,
		TYPE_CPLX32 = 0x0050,
		TYPE_PCPLX32 = 0x0150,
		TYPE_PFCPLX32 = 0x0250,
		TYPE_PHCPLX32 = 0x0350,
		TYPE_32PCPLX32 = 0x0450,
		TYPE_32PFCPLX32 = 0x0550,
		TYPE_64PCPLX32 = 0x0650,
		TYPE_CPLX64 = 0x0051,
		TYPE_PCPLX64 = 0x0151,
		TYPE_PFCPLX64 = 0x0251,
		TYPE_PHCPLX64 = 0x0351,
		TYPE_32PCPLX64 = 0x0451,
		TYPE_32PFCPLX64 = 0x0551,
		TYPE_64PCPLX64 = 0x0651,
		TYPE_CPLX80 = 0x0052,
		TYPE_PCPLX80 = 0x0152,
		TYPE_PFCPLX80 = 0x0252,
		TYPE_PHCPLX80 = 0x0352,
		TYPE_32PCPLX80 = 0x0452,
		TYPE_32PFCPLX80 = 0x0552,
		TYPE_64PCPLX80 = 0x0652,
		TYPE_CPLX128 = 0x0053,
		TYPE_PCPLX128 = 0x0153,
		TYPE_PFCPLX128 = 0x0253,
		TYPE_PHCPLX128 = 0x0353,
		TYPE_32PCPLX128 = 0x0453,
		TYPE_32PFCPLX128 = 0x0553,
		TYPE_64PCPLX128 = 0x0653,
		TYPE_BOOL08 = 0x0030,
		TYPE_PBOOL08 = 0x0130,
		TYPE_PFBOOL08 = 0x0230,
		TYPE_PHBOOL08 = 0x0330,
		TYPE_32PBOOL08 = 0x0430,
		TYPE_32PFBOOL08 = 0x0530,
		TYPE_64PBOOL08 = 0x0630,
		TYPE_BOOL16 = 0x0031,
		TYPE_PBOOL16 = 0x0131,
		TYPE_PFBOOL16 = 0x0231,
		TYPE_PHBOOL16 = 0x0331,
		TYPE_32PBOOL16 = 0x0431,
		TYPE_32PFBOOL16 = 0x0531,
		TYPE_64PBOOL16 = 0x0631,
		TYPE_BOOL32 = 0x0032,
		TYPE_PBOOL32 = 0x0132,
		TYPE_PFBOOL32 = 0x0232,
		TYPE_PHBOOL32 = 0x0332,
		TYPE_32PBOOL32 = 0x0432,
		TYPE_32PFBOOL32 = 0x0532,
		TYPE_64PBOOL32 = 0x0632,
		TYPE_BOOL64 = 0x0033,
		TYPE_PBOOL64 = 0x0133,
		TYPE_PFBOOL64 = 0x0233,
		TYPE_PHBOOL64 = 0x0333,
		TYPE_32PBOOL64 = 0x0433,
		TYPE_32PFBOOL64 = 0x0533,
		TYPE_64PBOOL64 = 0x0633,
		TYPE_NCVPTR = 0x01f0,
		TYPE_FCVPTR = 0x02f0,
		TYPE_HCVPTR = 0x03f0,
		TYPE_32NCVPTR = 0x04f0,
		TYPE_32FCVPTR = 0x05f0,
		TYPE_64NCVPTR = 0x06f0
	};

	enum class EPrimitiveMode
	{
		MODE_DIRECT = 0,
		MODE_NEAR_PTR = 1,
		MODE_FAR_PTR = 2,
		MODE_HUGE_PTR = 3,
		MODE_NEAR_PTR32 = 4,
		MODE_FAR_PTR32 = 5,
		MODE_NEAR_PTR64 = 6,
		MODE_NEAR_PTR128 = 7
	};

	enum class EPrimitiveType
	{
		TYPE_SPECIAL = 0,
		TYPE_SIGNED = 1,
		TYPE_UNSIGNED = 2,
		TYPE_BOOLEAN = 3,
		TYPE_REAL = 4,
		TYPE_COMPLEX = 5,
		TYPE_SPECIAL2 = 6,
		TYPE_INT = 7,
		TYPE_RESERVED = 8
	};

	enum class EPrimitiveSubtypeSpecial
	{
		SPECIAL_NOTYPE = 0,
		SPECIAL_ABS = 1,
		SPECIAL_SEGMENT = 2,
		SPECIAL_VOID = 3,
		SPECIAL_CURRENCY = 4,
		SPECIAL_NEAR_BASIC_STR = 5,
		SPECIAL_FAR_BASIC_STR = 6,
		SPECIAL_NO_TRANSLATION = 7,
		SPECIAL_HRESULT = 8
	};

	enum class EPrimitiveSubtypeSpecial2
	{
		SPECIAL2_BIT = 0,
		SPECIAL2_PASCAL_CHAR = 1,
		SPECIAL2_BOOL32FF = 2
	};

	enum class EPrimitiveSubtypeIntegral
	{
		INTEGRAL_1BYTE = 0,
		INTEGRAL_2BYTE = 1,
		INTEGRAL_4BYTE = 2,
		INTEGRAL_8BYTE = 3,
		INTEGRAL_16BYTE = 4,
	};

	enum class EPrimitiveSubtypeReal
	{
		REAL_32 = 0,
		REAL_64 = 1,
		REAL_80 = 2,
		REAL_128 = 3,
		REAL_48 = 4,
		REAL_32PP = 5,
		REAL_16 = 6,
	};

	enum class EPrimitiveSubtypeInt
	{
		INT_CHAR = 0,
		INT_INT1 = 0,
		INT_WCHAR = 1,
		INT_UINT1 = 1,
		INT_INT2 = 2,
		INT_UINT2 = 3,
		INT_INT4 = 4,
		INT_UINT4 = 5,
		INT_INT8 = 6,
		INT_UINT8 = 7,
		INT_INT16 = 8,
		INT_UINT16 = 9,
		INT_CHAR16 = 10,
		INT_CHAR32 = 11
	};

	enum class EPointerType : uint8_t
	{
		TYPE_NEAR16 = 0x00,
		TYPE_FAR16 = 0x01,
		TYPE_HUGE16 = 0x02,
		TYPE_BASE_ON_SEGMENT = 0x03,
		TYPE_BASE_ON_VALUE = 0x04,
		TYPE_BASE_ON_SEGMENT_VALUE = 0x05,
		TYPE_BASE_ON_ADDRESS = 0x06,
		TYPE_BASE_ON_SEGMENT_ADDRESS = 0x07,
		TYPE_BASE_ON_TYPE = 0x08,
		TYPE_BASE_ON_SELF = 0x09,
		TYPE_NEAR32 = 0x0a,
		TYPE_FAR32 = 0x0b,
		TYPE_NEAR64 = 0x0c
	};

	enum class EPointerMode : uint8_t
	{
		MODE_POINTER = 0x00,
		MODE_LVALUE_REF = 0x01,
		MODE_POINTER_TO_DATA_MEMBER = 0x02,
		MODE_POINTER_TO_MEMBER_FUNCTION = 0x03,
		MODE_RVALUE_REF = 0x04
	};

	enum class EMemberAccessType : uint8_t
	{
		ACCESS_NO_PROTECTION,
		ACCESS_PRIVATE,
		ACCESS_PROTECTED,
		ACCESS_PUBLIC
	};

	enum class EMemberPropertyType : uint8_t
	{
		PROPERTY_VANILLA,
		PROPERTY_VIRTUAL,
		PROPERTY_STATIC,
		PROPERTY_FRIEND,
		PROPERTY_INTRO_VIRTUAL_METHOD,
		PROPERTY_PURE_VIRTUAL_METHOD,
		PROPERTY_PURE_INTRO_VIRTUAL_METHOD
	};

#pragma pack(push, 1)
	struct MemberAttribField
	{
		EMemberAccessType access : 2;
		EMemberPropertyType property : 3;
		bool bCompilerInstantiated : 1;
		bool bNoInherit : 1;
		bool bNoConstruct : 1;
		bool bCompilerGenerated : 1;
		bool bSealed : 1;
		uint8_t unused : 6;
	};
#pragma pack(pop)

	struct ByteBuffer
	{
		ByteBuffer() = default;
		ByteBuffer(uint64_t poolIndex, bool isLocal) : poolIndex(poolIndex), isLocal(isLocal) {}

		const char* str() const;
		uint8_t u8() const;
		uint16_t u16() const;
		uint32_t u32() const;
		int8_t s8() const;
		int16_t s16() const;
		int32_t s32() const;
		uint32_t len() const;

		uint64_t getHash() const { return poolIndex; }
		const char* operator()() const { return str(); }

	private:
		friend struct ByteBufferPool;
		uint64_t poolIndex = 0;
		bool isLocal = true;
	};

	enum class EHFAType : uint8_t
	{
		HFA_NONE,
		HFA_FLOAT,
		HFA_DOUBLE,
		HFA_OTHER
	};

	enum class EMoCOMType : uint8_t
	{
		MOCOM_UDT_NONE,
		MOCOM_UDT_REF,
		MOCOM_UDT_VALUE,
		MOCOM_UDT_INTERFACE
	};

	struct LeafPropertyField
	{
		bool bPacked : 1;
		bool bHasCtorDtor : 1;
		bool bHasOverloadOps : 1;
		bool bIsNestedClass : 1;
		bool bHasNestedClass : 1;
		bool bOverloadsAssingment : 1;
		bool bHasCastingMethod : 1;
		bool bIsForwardRef : 1;
		bool bIsScopedDefinition : 1;
		bool bHasUniqueName : 1;
		bool bSealed : 1;
		EHFAType hfa : 2;
		bool bIntrinsic : 1;
		EMoCOMType moCOM : 2;
	};

	enum class ENumericType : uint16_t
	{
		NUM_INLINE16 = 0x0000,
		NUM_SINT8 = 0x8000,
		NUM_SINT16 = 0x8001,
		NUM_UINT16 = 0x8002,
		NUM_SINT32 = 0x8003,
		NUM_UINT32 = 0x8004,
		NUM_FLOAT32 = 0x8005,
		NUM_FLOAT64 = 0x8006,
		NUM_FLOAT80 = 0x8007,
		NUM_FLOAT128 = 0x8008,
		NUM_SINT64 = 0x8009,
		NUM_UINT64 = 0x800A,
		NUM_FLOAT48 = 0x800B,
		NUM_COMPLEX32 = 0x800C,
		NUM_COMPLEX64 = 0x800D,
		NUM_COMPLEX80 = 0x800E,
		NUM_COMPLEX128 = 0x800F,
		NUM_VARSTR = 0x8010
	};

	struct NumericType
	{
		ENumericType type;
		ByteBuffer value;

		uint8_t u8() const { return value.u8(); }
		uint16_t u16() const { return value.u16(); }
		uint32_t u32() const { return value.u32(); }
		uint8_t s8() const { return value.s8(); }
		uint16_t s16() const { return value.s16(); }
		uint32_t s32() const { return value.s32(); }
		uint32_t operator()() const { return u32(); }
	};

	enum class EPDBTpiStreamVersion : uint32_t
	{
		V40 = 19950410,
		V41 = 19951122,
		V50 = 19961031,
		V70 = 19990903,
		V80 = 20040203
	};


	struct PDBTpiStreamHeader
	{
		EPDBTpiStreamVersion version;
		uint32_t headerSize;
		uint32_t typeIndexBegin;
		uint32_t typeIndexEnd;
		uint32_t typeRecordBytes;
		uint16_t hashStreamIndex;
		uint16_t hashAuxStreamIndex;
		uint32_t hashKeySize;
		uint32_t numHashBuckets;
		int32_t hashValueBufferOffset;
		uint32_t hashValueBufferLength;
		int32_t indexOffsetBufferOffset;
		uint32_t indexOffsetBufferLength;
		int32_t hashAdjBufferOffset;
		uint32_t hashAdjBufferLength;
	};





	static size_t pointerSize = sizeof(void*);
	typedef void(*ErrorFunctionType)(void*, size_t);
	static ErrorFunctionType globalErrorFunction = nullptr;



	enum class EPDBResult
	{
		RESULT_SUCCESS,
		RESULT_FAIL_MSF_MAGIC,
		RESULT_FAIL_MSF_SUPERBLOCK,
		RESULT_FAIL_MSF_STREAM_DIRECTORY,
		RESULT_FAIL_TPI_HEADER_SIZE,
		RESULT_FAIL_TPI_HEADER_HASH_BUFFER_LEN,
		RESULT_FAIL_DBI_INVALID_PLATFORM
	};

	const char* pdbResultToStr(EPDBResult result)
	{
		switch (result)
		{
		case EPDBResult::RESULT_SUCCESS: return "Success";
		case EPDBResult::RESULT_FAIL_MSF_MAGIC: return "Invalid MSF Header (Invalid Magic)";
		case EPDBResult::RESULT_FAIL_MSF_SUPERBLOCK: return "Invalid MSF Header (Invalid Superblock)";
		case EPDBResult::RESULT_FAIL_MSF_STREAM_DIRECTORY: return "Invalid MSF Header (Invalid Stream Directory)";
		case EPDBResult::RESULT_FAIL_TPI_HEADER_SIZE: return "Invalid TPI Header (Invalid Header Size)";
		case EPDBResult::RESULT_FAIL_TPI_HEADER_HASH_BUFFER_LEN: return "Invalid TPI Header (Invalid Hash Buffer Length)";
		case EPDBResult::RESULT_FAIL_DBI_INVALID_PLATFORM: return "Invalid DBI Header (Invalid Platform)";
		default: break;
		}
		return "Unknown";
	}

	struct WriteBuffer
	{
		WriteBuffer(uint64_t initialSize) :
			buffer()
		{
		}

		void append(const char* pFmt, ...)
		{
			static char tmpBuffer[4096 * 3] = {};
			static uint32_t index = 0;
			char* currBuffer = &tmpBuffer[index * 4096];
			va_list vaArgs;
			va_start(vaArgs, pFmt);
			int32_t written = vsprintf_s(currBuffer, 4096, pFmt, vaArgs);
			buffer.add(currBuffer, written);
			va_end(vaArgs);
		}

		void flush(const char* pFileName, const char* pFileAppendName, const char* pFileExtension, void* flushFunction = nullptr)
		{
			HANDLE outputFile = CreateFileA(TempFmtByteBuffer("%s%s.%s", pFileName, pFileAppendName, pFileExtension), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (outputFile == INVALID_HANDLE_VALUE)
			{
				ALERT("Failed to create output file %s", pFileName);
				return;
			}
			DWORD writtenBytes = 0;
			if (!WriteFile(outputFile, getData(), (DWORD)getSizeInBytes(), &writtenBytes, nullptr) || writtenBytes != getSizeInBytes())
			{
				ALERT("Failed to write output file %s", pFileName);
				return;
			}
			CloseHandle(outputFile);
			if (flushFunction != nullptr)
			{
				((void(*)(const char*, uint32_t))flushFunction)(buffer.getData(), (uint32_t)buffer.getNumInByteSize());
			}
			buffer.clear();
		}

		const char* getData() { return buffer.getData(); }
		uint64_t getSizeInBytes() const { return buffer.getNumInByteSize(); }

	private:
		TArray<char> buffer;
	};

	static size_t murmurHash(const void* key, size_t len, size_t seed)
	{
		const uint64_t m = 0xc6a4a7935bd1e995ULL;
		const uint32_t r = 47;

		uint64_t h = seed ^ (len * m);

		const uint64_t* data = (const uint64_t*)key;
		const uint64_t* end = data + (len / 8);

		while (data != end)
		{
			uint64_t k = *data++;
			k *= m;
			k ^= k >> r;
			k *= m;
			h ^= k;
			h *= m;
		}

		const unsigned char* data2 = (const unsigned char*)data;

		switch (len & 7)
		{
		case 7: h ^= uint64_t(data2[6]) << 48;
		case 6: h ^= uint64_t(data2[5]) << 40;
		case 5: h ^= uint64_t(data2[4]) << 32;
		case 4: h ^= uint64_t(data2[3]) << 24;
		case 3: h ^= uint64_t(data2[2]) << 16;
		case 2: h ^= uint64_t(data2[1]) << 8;
		case 1: h ^= uint64_t(data2[0]);
			h *= m;
		};

		h ^= h >> r;
		h *= m;
		h ^= h >> r;

		return h;
	}

	struct UDTTable
	{
		static UDTTable& get()
		{
			static UDTTable instance;
			return instance;
		}

		struct Entry
		{
			ByteBuffer name;
			uint32_t typeIndex = 0;
			uint32_t count = 0;
		};

		void add(ByteBuffer name, uint32_t typeIndex)
		{
			ASSERT(typeIndex > 0x1000u, "Can't add type smaller than 0x1000");
			size_t hash = murmurHash(name(), name.len(), (size_t)0xBADC0FFEE);
			if (nameMap.find(hash) == nameMap.end())
			{
				nameMap[hash] = { name, typeIndex };
			}
			else
			{
				Entry& n = nameMap[hash];
				if (strcmp(name(), n.name()) == 0)
					n.count += 1;
			}
		}

		uint32_t getTypeIndex(ByteBuffer name)
		{
			return nameMap[murmurHash(name(), name.len(), (size_t)0xBADC0FFEE)].typeIndex;
		}

		bool hasName(ByteBuffer name)
		{
			return nameMap.find(murmurHash(name(), name.len(), (size_t)0xBADC0FFEE)) != nameMap.end();
		}

		void clear()
		{
			nameMap.clear();
		}

	private:
		std::unordered_map<size_t, Entry> nameMap;
	};

	struct IPIStringTable
	{
		static IPIStringTable& get()
		{
			static IPIStringTable instance;
			return instance;
		}

		void setNamesBuffer(const uint8_t* buffer, uint32_t size)
		{
			namesBuffer.clear();
			namesBuffer.add((char*)buffer, size);
		}

		String getByOffset(uint32_t offset)
		{
			if (offset >= (uint32_t)namesBuffer.getNum())
				return "";
			const char* str = namesBuffer.getData() + offset;
			return String(str);
		}

		void add(uint32_t typeIndex, const String& str)
		{
			stringMap[typeIndex] = str;
		}

		String get(uint32_t typeIndex)
		{
			auto it = stringMap.find(typeIndex);
			if (it != stringMap.end())
				return it->second;
			return "";
		}

		void clear()
		{
			stringMap.clear();
			namesBuffer.clear();
		}

	private:
		std::unordered_map<uint32_t, String> stringMap;
		TArray<char> namesBuffer;
	};

	struct UDTSrcLineTable
	{
		static UDTSrcLineTable& get()
		{
			static UDTSrcLineTable instance;
			return instance;
		}

		struct Entry
		{
			uint32_t srcFileIndex = 0;
			uint32_t lineNumber = 0;
		};

		void add(uint32_t udtTypeIndex, uint32_t srcFileIndex, uint32_t lineNumber)
		{
			entryMap[udtTypeIndex] = { srcFileIndex, lineNumber };
		}

		bool get(uint32_t udtTypeIndex, Entry& output)
		{
			auto it = entryMap.find(udtTypeIndex);
			if (it != entryMap.end())
			{
				output = it->second;
				return true;
			}
			return false;
		}

		void clear()
		{
			entryMap.clear();
		}

	private:
		std::unordered_map<uint32_t, Entry> entryMap;
	};

	struct ByteBufferPool
	{
		static ByteBufferPool& get()
		{
			static ByteBufferPool bufferPool;
			return bufferPool;
		}

		ByteBufferPool() : pool() {}

		void clear()
		{
			pool.clear();
		}

		ByteBuffer add(const uint8_t* pBuffer, uint32_t len, bool nullTerm = false)
		{
			if (len > 4 || nullTerm)
			{
				ByteBuffer bytes;
				bytes.poolIndex = pool.getNum();
				bytes.isLocal = false;
				pool.add((char*)&len, sizeof(len));
				pool.add((char*)pBuffer, len);
				if (nullTerm) pool.add(0);
				return bytes;
			}
			else
			{
				ByteBuffer bytes;
				memcpy(&bytes.poolIndex, pBuffer, len);
				uint64_t encLen = ((uint64_t)len << 32);
				bytes.poolIndex |= encLen;
				bytes.isLocal = true;

#if _DEBUG
				uint32_t v = bytes.u32();
				uint32_t l = bytes.len();
				ASSERT(l == len, "Corrupted Len");
				ASSERT(memcmp(&v, pBuffer, l) == 0, "Corrupted Data");
#endif
				return bytes;
			}
		}

		template<class T>
		ByteBuffer tAdd(const T* pBuffer, uint32_t count, bool nullTerm = false)
		{
			return add((const uint8_t*)pBuffer, count * sizeof(T), nullTerm);
		}

		const uint8_t* getByteBuffer(uint64_t index) { return (uint8_t*)&pool[index + 4]; }
		const char* getStr(uint64_t index) { return &pool[index + 4]; }
		uint8_t getU8(uint64_t index) { return bufferReadU8(getByteBuffer(index)); }
		uint16_t getU16(uint64_t index) { return bufferReadU16(getByteBuffer(index)); }
		uint32_t getU32(uint64_t index) { return bufferReadU32(getByteBuffer(index)); }
		int8_t getS8(uint64_t index) { return bufferReadS8(getByteBuffer(index)); }
		int16_t getS16(uint64_t index) { return bufferReadS16(getByteBuffer(index)); }
		int32_t getS32(uint64_t index) { return bufferReadS32(getByteBuffer(index)); }
		uint32_t getLen(uint64_t index) { return bufferReadU32((const uint8_t*)&pool[index]); }

	private:
		TArray<char> pool;
	};

	const char* ByteBuffer::str() const { return !isLocal ? ByteBufferPool::get().getStr(poolIndex) : nullptr; }
	uint8_t ByteBuffer::u8() const { return !isLocal ? ByteBufferPool::get().getU8(poolIndex) : (uint8_t)poolIndex; }
	uint16_t ByteBuffer::u16() const { return !isLocal ? ByteBufferPool::get().getU16(poolIndex) : (uint16_t)poolIndex; }
	uint32_t ByteBuffer::u32() const { return !isLocal ? ByteBufferPool::get().getU32(poolIndex) : (uint32_t)poolIndex; }
	int8_t ByteBuffer::s8() const { return !isLocal ? ByteBufferPool::get().getS8(poolIndex) : *((int8_t*)&poolIndex); }
	int16_t ByteBuffer::s16() const { return !isLocal ? ByteBufferPool::get().getS16(poolIndex) : *((int8_t*)&poolIndex); }
	int32_t ByteBuffer::s32() const { return !isLocal ? ByteBufferPool::get().getS32(poolIndex) : *((int32_t*)&poolIndex); }
	uint32_t ByteBuffer::len() const { return !isLocal ? ByteBufferPool::get().getLen(poolIndex) : (uint32_t)((poolIndex & 0xFFFFFFFF00000000) >> 32); }


	struct BufferStreamReader : public IStreamReader
	{
		BufferStreamReader(const uint8_t* pBuffer, uint64_t bufferSize = 0) :
			pBuffer(pBuffer),
			readOffset(0),
			bufferSize(bufferSize)
		{
		}

		virtual const uint8_t* getBuffer() override { return pBuffer; }
		virtual uint64_t& getReadOffset() override { return readOffset; }

	private:
		const uint8_t* pBuffer;
		uint64_t readOffset;
		uint64_t bufferSize;
	};

	inline uint32_t getNumBlocks(uint32_t blockSize, uint32_t size)
	{
		return (size / blockSize) + ((size % blockSize) ? 1 : 0);
	}

	EPDBResult readMSFSuperBlock(const uint8_t* pInputData, uint64_t dataSize, MSFSuperBlock& output)
	{
		BufferStreamReader stream(pInputData, dataSize);
		MSFSuperBlock superBlock = {};
		stream.readBuffer(superBlock.fileMagic, sizeof(MSF7Magic));
		if (memcmp(superBlock.fileMagic, MSF7Magic, sizeof(MSF7Magic)) != 0)
		{
			return EPDBResult::RESULT_FAIL_MSF_MAGIC;
		}
		superBlock.blockSize = stream.readU32();
		superBlock.freeBlockMapBlock = stream.readU32();
		superBlock.numBlocks = stream.readU32();
		superBlock.numDirectoryBytes = stream.readU32();
		superBlock.unknown = stream.readU32();
		superBlock.blockMapAddr = stream.readU32();

		if (superBlock.numBlocks * superBlock.blockSize != (uint32_t)dataSize)
		{
			return EPDBResult::RESULT_FAIL_MSF_SUPERBLOCK;
		}

		output = superBlock;
		return EPDBResult::RESULT_SUCCESS;
	}

	EPDBResult readMSFStreamDirectory(const uint8_t* pInputData, uint64_t dataSize, const MSFSuperBlock& superBlock, MSFStreamDirectory& output)
	{
		MSFStreamDirectory streamDirectory = {};
		uint32_t streamStartOffset = superBlock.blockMapAddr * superBlock.blockSize;
		uint32_t indexOffset = bufferReadU32((pInputData + streamStartOffset));
		BufferStreamReader stream(pInputData + ((uint64_t)indexOffset * superBlock.blockSize));
		streamDirectory.numStreams = stream.readU32();
		streamDirectory.pStreamSizes = mallocType<uint32_t>(streamDirectory.numStreams);
		streamDirectory.pStreamBlocks = mallocType<MSFStreamBlock>(streamDirectory.numStreams);
		const uint8_t* pStreamBlocksBuffer = stream.getBufferCurr() + (streamDirectory.numStreams * sizeof(uint32_t));
		uint32_t u32Count = 1 + streamDirectory.numStreams;

		for (uint32_t index = 0; index < streamDirectory.numStreams; ++index)
		{
			uint32_t streamSize = stream.readU32();
			if (streamSize != 0xFFFFFFFF)
			{
				uint32_t blockCount = getNumBlocks(superBlock.blockSize, streamSize);
				streamDirectory.pStreamSizes[index] = streamSize;
				streamDirectory.pStreamBlocks[index].pBlocks = mallocType<uint32_t>(blockCount);
				streamDirectory.pStreamBlocks[index].blockCount = blockCount;
				for (uint32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex)
				{
					uint32_t block = bufferReadU32(pStreamBlocksBuffer + (blockIndex * sizeof(uint32_t)));
					streamDirectory.pStreamBlocks[index].pBlocks[blockIndex] = block;
				}
				pStreamBlocksBuffer += blockCount * sizeof(uint32_t);
				u32Count += blockCount;
			}
		}

		if (u32Count * sizeof(uint32_t) != superBlock.numDirectoryBytes)
		{
			return EPDBResult::RESULT_FAIL_MSF_STREAM_DIRECTORY;
		}

		output = streamDirectory;
		return EPDBResult::RESULT_SUCCESS;
	}

	PDBMergedBlocks readBlocks(const uint8_t* pInputData, uint64_t dataSize, MSFSuperBlock superBlock, MSFStreamBlock streamBlock)
	{
		uint8_t* pReadBuffer = mallocType<uint8_t>((uint64_t)streamBlock.blockCount * superBlock.blockSize);
		uint8_t* pCurrBuffer = pReadBuffer;
		for (uint32_t index = 0; index < streamBlock.blockCount; ++index)
		{
			memcpy(pCurrBuffer, pInputData + (uint64_t)streamBlock.pBlocks[index] * superBlock.blockSize, superBlock.blockSize);
			pCurrBuffer += superBlock.blockSize;
		}
		return PDBMergedBlocks{ (uint64_t)streamBlock.blockCount * superBlock.blockSize, pReadBuffer, streamBlock.blockCount };
	}

	void freeBlocks(PDBMergedBlocks& block)
	{
		free((void*)(block.pBuffer));
		block.pBuffer = nullptr;
		block.blockCount = 0;
		block.blockSizeInBytes = 0;
	}

	static size_t getPrimitiveSizeType(uint32_t typeIndex)
	{
		EPrimitiveType type = (EPrimitiveType)((typeIndex >> 4) & 0b1111);
		EPrimitiveMode mode = (EPrimitiveMode)((typeIndex >> 8) & 0b111);
		uint8_t subtype = typeIndex & 0b111;

		if (mode == EPrimitiveMode::MODE_DIRECT)
		{
			if (type == EPrimitiveType::TYPE_SIGNED ||
				type == EPrimitiveType::TYPE_UNSIGNED ||
				type == EPrimitiveType::TYPE_BOOLEAN)
			{
				EPrimitiveSubtypeIntegral integralType = (EPrimitiveSubtypeIntegral)subtype;
				switch (integralType)
				{
				case EPrimitiveSubtypeIntegral::INTEGRAL_1BYTE: return 1;
				case EPrimitiveSubtypeIntegral::INTEGRAL_2BYTE: return 2;
				case EPrimitiveSubtypeIntegral::INTEGRAL_4BYTE: return 4;
				case EPrimitiveSubtypeIntegral::INTEGRAL_8BYTE: return 8;
				case EPrimitiveSubtypeIntegral::INTEGRAL_16BYTE: return 16;
				}
				ALERT("Invalid integral subtype 0x%04X", subtype);
			}
			else if (type == EPrimitiveType::TYPE_REAL || type == EPrimitiveType::TYPE_COMPLEX)
			{
				EPrimitiveSubtypeReal realType = (EPrimitiveSubtypeReal)subtype;
				switch (realType)
				{
				case EPrimitiveSubtypeReal::REAL_32: return 4;
				case EPrimitiveSubtypeReal::REAL_64: return 8;
				case EPrimitiveSubtypeReal::REAL_80: return 10;
				case EPrimitiveSubtypeReal::REAL_128: return 16;
				case EPrimitiveSubtypeReal::REAL_48: return 6;
				case EPrimitiveSubtypeReal::REAL_32PP: return 4;
				case EPrimitiveSubtypeReal::REAL_16: return 2;
				}
				ALERT("Invalid real subtype 0x%04X", subtype);
			}
			else if (type == EPrimitiveType::TYPE_INT)
			{
				EPrimitiveSubtypeInt intType = (EPrimitiveSubtypeInt)subtype;
				switch (intType)
				{
				case EPrimitiveSubtypeInt::INT_CHAR: return 1;
				case EPrimitiveSubtypeInt::INT_UINT1: return 1;
				case EPrimitiveSubtypeInt::INT_INT2: return 2;
				case EPrimitiveSubtypeInt::INT_UINT2: return 2;
				case EPrimitiveSubtypeInt::INT_INT4: return 4;
				case EPrimitiveSubtypeInt::INT_UINT4: return 4;
				case EPrimitiveSubtypeInt::INT_INT8: return 8;
				case EPrimitiveSubtypeInt::INT_UINT8: return 8;
				case EPrimitiveSubtypeInt::INT_INT16: return 16;
				case EPrimitiveSubtypeInt::INT_UINT16: return 16;
				case EPrimitiveSubtypeInt::INT_CHAR16: return 2;
				case EPrimitiveSubtypeInt::INT_CHAR32: return 4;
				}
				ALERT("Invalid int subtype 0x%04X", subtype);
			}
			else if (type == EPrimitiveType::TYPE_SPECIAL)
			{
				EPrimitiveSubtypeSpecial specialType = (EPrimitiveSubtypeSpecial)subtype;
				switch (specialType)
				{
				case EPrimitiveSubtypeSpecial::SPECIAL_NOTYPE: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_ABS: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_SEGMENT: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_VOID: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_CURRENCY: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_NEAR_BASIC_STR: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_FAR_BASIC_STR: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_NO_TRANSLATION: return 0;
				case EPrimitiveSubtypeSpecial::SPECIAL_HRESULT: return 4;
				}
				ALERT("Invalid special subtype 0x%04X", subtype);
			}
			else if (type == EPrimitiveType::TYPE_SPECIAL2)
			{
				EPrimitiveSubtypeSpecial2 specialType = (EPrimitiveSubtypeSpecial2)subtype;
				switch (specialType)
				{
				case EPrimitiveSubtypeSpecial2::SPECIAL2_BIT: return 0;
				case EPrimitiveSubtypeSpecial2::SPECIAL2_PASCAL_CHAR: return 1;
				case EPrimitiveSubtypeSpecial2::SPECIAL2_BOOL32FF: return 4;
				}
				ALERT("Invalid special 2 subtype 0x%04X", subtype);
			}
		}
		else
		{
			switch (mode)
			{
			case EPrimitiveMode::MODE_NEAR_PTR: return 2;
			case EPrimitiveMode::MODE_FAR_PTR: return 2;
			case EPrimitiveMode::MODE_HUGE_PTR: return 4;
			case EPrimitiveMode::MODE_NEAR_PTR32: return 4;
			case EPrimitiveMode::MODE_FAR_PTR32: return 4;
			case EPrimitiveMode::MODE_NEAR_PTR64: return 8;
			case EPrimitiveMode::MODE_NEAR_PTR128: return 16;
			case EPrimitiveMode::MODE_DIRECT: break;
			}
			ALERT("Invalid type index mode 0x%04X", (uint16_t)mode);
		}

		ALERT("Invalid primitive type index 0x%08X", typeIndex);
		return 0;
	}

	static bool getPrimitiveTypeIsPtr(uint32_t typeIndex)
	{
		EPrimitiveMode mode = (EPrimitiveMode)((typeIndex >> 8) & 0b111);
		return mode != EPrimitiveMode::MODE_DIRECT;
	}

	static const char* getPrimitiveName(uint32_t typeIndex)
	{
		switch ((EPrimitiveBasicType)typeIndex)
		{
		case EPrimitiveBasicType::TYPE_NOTYPE: return "notype";
		case EPrimitiveBasicType::TYPE_ABS: return "abs";
		case EPrimitiveBasicType::TYPE_SEGMENT: return "segment";
		case EPrimitiveBasicType::TYPE_VOID: return "void";
		case EPrimitiveBasicType::TYPE_HRESULT: return "hresult";
		case EPrimitiveBasicType::TYPE_32PHRESULT: return "32phresult";
		case EPrimitiveBasicType::TYPE_64PHRESULT: return "64phresult";
		case EPrimitiveBasicType::TYPE_PVOID: return "pvoid";
		case EPrimitiveBasicType::TYPE_PFVOID: return "pfvoid";
		case EPrimitiveBasicType::TYPE_PHVOID: return "phvoid";
		case EPrimitiveBasicType::TYPE_32PVOID: return "32pvoid";
		case EPrimitiveBasicType::TYPE_32PFVOID: return "32pfvoid";
		case EPrimitiveBasicType::TYPE_64PVOID: return "64pvoid";
		case EPrimitiveBasicType::TYPE_CURRENCY: return "currency";
		case EPrimitiveBasicType::TYPE_NBASICSTR: return "nbasicstr";
		case EPrimitiveBasicType::TYPE_FBASICSTR: return "fbasicstr";
		case EPrimitiveBasicType::TYPE_NOTTRANS: return "nottrans";
		case EPrimitiveBasicType::TYPE_BIT: return "bit";
		case EPrimitiveBasicType::TYPE_PASCHAR: return "paschar";
		case EPrimitiveBasicType::TYPE_BOOL32FF: return "bool32ff";
		case EPrimitiveBasicType::TYPE_CHAR: return "char";
		case EPrimitiveBasicType::TYPE_PCHAR: return "pchar";
		case EPrimitiveBasicType::TYPE_PFCHAR: return "pfchar";
		case EPrimitiveBasicType::TYPE_PHCHAR: return "phchar";
		case EPrimitiveBasicType::TYPE_32PCHAR: return "32pchar";
		case EPrimitiveBasicType::TYPE_32PFCHAR: return "32pfchar";
		case EPrimitiveBasicType::TYPE_64PCHAR: return "64pchar";
		case EPrimitiveBasicType::TYPE_UCHAR: return "uchar";
		case EPrimitiveBasicType::TYPE_PUCHAR: return "puchar";
		case EPrimitiveBasicType::TYPE_PFUCHAR: return "pfuchar";
		case EPrimitiveBasicType::TYPE_PHUCHAR: return "phuchar";
		case EPrimitiveBasicType::TYPE_32PUCHAR: return "32puchar";
		case EPrimitiveBasicType::TYPE_32PFUCHAR: return "32pfuchar";
		case EPrimitiveBasicType::TYPE_64PUCHAR: return "64puchar";
		case EPrimitiveBasicType::TYPE_RCHAR: return "rchar";
		case EPrimitiveBasicType::TYPE_PRCHAR: return "prchar";
		case EPrimitiveBasicType::TYPE_PFRCHAR: return "pfrchar";
		case EPrimitiveBasicType::TYPE_PHRCHAR: return "phrchar";
		case EPrimitiveBasicType::TYPE_32PRCHAR: return "32prchar";
		case EPrimitiveBasicType::TYPE_32PFRCHAR: return "32pfrchar";
		case EPrimitiveBasicType::TYPE_64PRCHAR: return "64prchar";
		case EPrimitiveBasicType::TYPE_WCHAR: return "wchar";
		case EPrimitiveBasicType::TYPE_PWCHAR: return "pwchar";
		case EPrimitiveBasicType::TYPE_PFWCHAR: return "pfwchar";
		case EPrimitiveBasicType::TYPE_PHWCHAR: return "phwchar";
		case EPrimitiveBasicType::TYPE_32PWCHAR: return "32pwchar";
		case EPrimitiveBasicType::TYPE_32PFWCHAR: return "32pfwchar";
		case EPrimitiveBasicType::TYPE_64PWCHAR: return "64pwchar";
		case EPrimitiveBasicType::TYPE_CHAR16: return "char16";
		case EPrimitiveBasicType::TYPE_PCHAR16: return "pchar16";
		case EPrimitiveBasicType::TYPE_PFCHAR16: return "pfchar16";
		case EPrimitiveBasicType::TYPE_PHCHAR16: return "phchar16";
		case EPrimitiveBasicType::TYPE_32PCHAR16: return "32pchar16";
		case EPrimitiveBasicType::TYPE_32PFCHAR16: return "32pfchar16";
		case EPrimitiveBasicType::TYPE_64PCHAR16: return "64pchar16";
		case EPrimitiveBasicType::TYPE_CHAR32: return "char32";
		case EPrimitiveBasicType::TYPE_PCHAR32: return "pchar32";
		case EPrimitiveBasicType::TYPE_PFCHAR32: return "pfchar32";
		case EPrimitiveBasicType::TYPE_PHCHAR32: return "phchar32";
		case EPrimitiveBasicType::TYPE_32PCHAR32: return "32pchar32";
		case EPrimitiveBasicType::TYPE_32PFCHAR32: return "32pfchar32";
		case EPrimitiveBasicType::TYPE_64PCHAR32: return "64pchar32";
		case EPrimitiveBasicType::TYPE_INT1: return "int8";
		case EPrimitiveBasicType::TYPE_UINT1: return "uint8";
		case EPrimitiveBasicType::TYPE_SHORT: return "short";
		case EPrimitiveBasicType::TYPE_USHORT: return "ushort";
		case EPrimitiveBasicType::TYPE_INT2: return "int16";
		case EPrimitiveBasicType::TYPE_UINT2: return "uint16";
		case EPrimitiveBasicType::TYPE_LONG: return "long";
		case EPrimitiveBasicType::TYPE_ULONG: return "ulong";
		case EPrimitiveBasicType::TYPE_INT4: return "int32";
		case EPrimitiveBasicType::TYPE_UINT4: return "uint32";
		case EPrimitiveBasicType::TYPE_QUAD: return "quad";
		case EPrimitiveBasicType::TYPE_UQUAD: return "uquad";
		case EPrimitiveBasicType::TYPE_INT8: return "int64";
		case EPrimitiveBasicType::TYPE_UINT8: return "uint64";
		case EPrimitiveBasicType::TYPE_REAL16: return "float16";
		case EPrimitiveBasicType::TYPE_REAL32: return "float";
		case EPrimitiveBasicType::TYPE_REAL32PP: return "float32pp";
		case EPrimitiveBasicType::TYPE_REAL48: return "float48";
		case EPrimitiveBasicType::TYPE_REAL64: return "double";
		case EPrimitiveBasicType::TYPE_REAL80: return "float80";
		case EPrimitiveBasicType::TYPE_REAL128: return "float128";
		case EPrimitiveBasicType::TYPE_BOOL08: return "bool";
		case EPrimitiveBasicType::TYPE_BOOL16: return "bool16";
		case EPrimitiveBasicType::TYPE_BOOL32: return "bool32";
		case EPrimitiveBasicType::TYPE_BOOL64: return "bool64";
		case EPrimitiveBasicType::TYPE_NCVPTR: return "ncvptr";
		case EPrimitiveBasicType::TYPE_FCVPTR: return "fcvptr";
		case EPrimitiveBasicType::TYPE_HCVPTR: return "hcvptr";
		case EPrimitiveBasicType::TYPE_32NCVPTR: return "32ncvptr";
		case EPrimitiveBasicType::TYPE_32FCVPTR: return "32fcvptr";
		case EPrimitiveBasicType::TYPE_64NCVPTR: return "64ncvptr";
		default: return "undefined";
		}
	}

	bool isMemberVirtual(EMemberPropertyType type)
	{
		return
			type == EMemberPropertyType::PROPERTY_INTRO_VIRTUAL_METHOD ||
			type == EMemberPropertyType::PROPERTY_PURE_VIRTUAL_METHOD ||
			type == EMemberPropertyType::PROPERTY_PURE_INTRO_VIRTUAL_METHOD;
	}

	uint64_t getNumericSize(ENumericType type)
	{
		switch (type)
		{
		case ENumericType::NUM_INLINE16: return 0;
		case ENumericType::NUM_SINT8: return 1;
		case ENumericType::NUM_SINT16: return 2;
		case ENumericType::NUM_UINT16: return 2;
		case ENumericType::NUM_SINT32: return 4;
		case ENumericType::NUM_UINT32: return 4;
		case ENumericType::NUM_FLOAT32: return 4;
		case ENumericType::NUM_FLOAT64: return 8;
		case ENumericType::NUM_FLOAT80: return 10;
		case ENumericType::NUM_FLOAT128: return 16;
		case ENumericType::NUM_SINT64: return 8;
		case ENumericType::NUM_UINT64: return 8;
		case ENumericType::NUM_FLOAT48: return 6;
		case ENumericType::NUM_COMPLEX32: return 8;
		case ENumericType::NUM_COMPLEX64: return 16;
		case ENumericType::NUM_COMPLEX80: return 20;
		case ENumericType::NUM_COMPLEX128: return 32;
		case ENumericType::NUM_VARSTR: return 0;
		default: break;
		}
		return 0;
	}

	uint64_t getNumericSizeWithHeader(ENumericType type)
	{
		switch (type)
		{
		case ENumericType::NUM_INLINE16: return 2;
		default: return getNumericSize(type) + 2;
		}
	}

	struct MemberRecord;
	struct MemberRecordBuffer
	{
		MemberRecordBuffer() = default;
		MemberRecordBuffer(uint64_t poolIndex, uint32_t count) : poolIndex(poolIndex), count(count) {}
		MemberRecord& operator[](uint32_t index);
		uint32_t getCount() const { return count; }

	private:
		friend struct MemberRecordTable;
		uint64_t poolIndex = 0;
		uint32_t count = 0;
	};

	struct MemberTypeEnumerate
	{
		MemberAttribField attrib;
		NumericType numeric;
		ByteBuffer name;
	};

	struct MemberTypeNestedType
	{
		uint32_t parentTypeIndex;
		ByteBuffer name;
	};

	struct MemberTypeBaseClass
	{
		MemberAttribField attrib;
		uint16_t pad;
		uint32_t classTypeIndex;
		NumericType subObjectOffset;
	};

	struct MemberTypeMethod
	{
		uint16_t overloads;
		uint32_t methodListTypeIndex;
		ByteBuffer name;
	};

	struct MemberTypeOneMethod
	{
		MemberAttribField attrib;
		uint16_t pad;
		uint32_t procedureTypeIndex;
		int32_t vTableOffset;
		ByteBuffer name;
	};

	struct MemberTypeDataMember
	{
		MemberAttribField attribField;
		uint16_t pad;
		uint32_t typeIndexRecordField;
		NumericType fieldOffset;
		ByteBuffer name;
	};

	struct MemberTypeStaticDataMember
	{
		MemberAttribField attribField;
		uint16_t pad;
		uint32_t typeIndexRecordField;
		ByteBuffer name;
	};

	struct MemberTypeVirtualFunctionTable
	{
		uint16_t padding;
		uint32_t pointerTypeIndex;
	};

	struct MemberTypeVirtualBaseClass
	{
		MemberAttribField attribField;
		uint16_t pad;
		uint32_t directVirtualBaseClassTypeIndex;
		uint32_t virtualBasePointerTypeIndex;
		NumericType vbPtrAddressOffset;
		NumericType vbPtrVTableOffset;
	};

	struct MemberTypeLeafIndex
	{
		uint16_t padding;
		uint32_t leafTypeIndex;
	};

	struct MemberRecord
	{
		ELeafType type;
		uint16_t  pad;
		union
		{
			MemberTypeEnumerate enumerate;
			MemberTypeNestedType nestedType;
			MemberTypeBaseClass baseClass;
			MemberTypeBaseClass baseInterface;
			MemberTypeMethod method;
			MemberTypeOneMethod oneMethod;
			MemberTypeDataMember dataMember;
			MemberTypeStaticDataMember staticDataMember;
			MemberTypeVirtualFunctionTable vTablePtr;
			MemberTypeVirtualBaseClass virtualBaseClass;
			MemberTypeLeafIndex leafIndex;
		};
	};

	struct MemberRecordTable
	{
		static MemberRecordTable& get()
		{
			static MemberRecordTable instance;
			return instance;
		}

		MemberRecordBuffer add(MemberRecord* pMemberRecordList, uint32_t len)
		{
			uint64_t index = records.getNum();
			records.add(pMemberRecordList, len);
			return MemberRecordBuffer(index, len);
		}

		MemberRecord* get(uint64_t index)
		{
			return &records[index];
		}

		void clear()
		{
			records.clear();
		}

	private:
		TArray<MemberRecord> records;
	};

	MemberRecord& MemberRecordBuffer::operator[](uint32_t index)
	{
		return MemberRecordTable::get().get(poolIndex)[index];
	}

	struct LeafTypeVFTable
	{
		uint32_t completeClass;
		uint32_t overriddenVFTable;
		uint32_t vfPtrOffset;
		uint32_t namesLen;
	};

	struct LeafTypeEnum
	{
		uint16_t enumNum;
		LeafPropertyField propertyField;
		uint32_t underlyingType;
		uint32_t fieldListTypeIndex;
		ByteBuffer name;
		ByteBuffer uniqueName;
	};

	struct ModifierAttributeField
	{
		bool bIsConst : 1;
		bool bIsVolatile : 1;
		bool bIsUnaligned : 1;
		uint16_t unused : 13;
	};

	struct LeafTypeModifier
	{
		ModifierAttributeField attrib;
		uint32_t typeIndex;
	};

	struct LeafTypeFieldList
	{
		uint16_t fieldNum;
		MemberRecordBuffer members;
	};

	struct PointerAttribField
	{
		EPointerType type : 5;
		EPointerMode mode : 3;
		struct
		{
			bool bIsFlatPtr : 1;
			bool bIsVolatile : 1;
			bool bIsConst : 1;
			bool bIsUnaligned : 1;
			bool bIsRestrict : 1;
		} modifier;
		unsigned long size : 6;
		struct
		{
			bool bIsMoCom : 1;
			bool bLValueRefThisPointer : 1;
			bool bRValueRefThisPointer : 1;
		} flags;
		uint32_t unused : 10;
	};

	struct LeafTypePointer
	{
		uint32_t underlyingType;
		PointerAttribField attrib;
	};

	enum class ECallingConvention : uint8_t
	{
		CALL_NEAR_C = 0,
		CALL_FAR_C = 1,
		CALL_NEAR_PASCAL = 2,
		CALL_FAR_PASCAL = 3,
		CALL_NEAR_FASTCALL = 4,
		CALL_FAR_FASTCALL = 5,
		CALL_RESERVED = 6,
		CALL_NEAR_STDCALL = 7,
		CALL_FAR_STDCALL = 8,
		CALL_NEAR_SYSCALL = 9,
		CALL_FAR_SYSCALL = 10,
		CALL_THIS_CALL = 11,
		CALL_MIPS_CALL = 12,
		CALL_GENERIC = 13,
		CALL_RESERVED_14_255 = 14
	};

	struct LeafTypeProcedure
	{
		uint32_t returnTypeIndex;
		ECallingConvention callingConvention;
		uint16_t paramNum;
		uint32_t argListTypeIndex;
	};

	struct MemberFunctionAttribField
	{
		bool bCXXReturnUDT : 1;
		bool bIsInstanceConstructor : 1;
		bool bIsInstanceConstructorVirtualBase : 1;
		uint8_t unused : 5;
	};

	struct LeafTypeMemberFunction
	{
		uint32_t returnTypeIndex;
		uint32_t classTypeIndex;
		uint32_t thisTypeIndex;
		ECallingConvention callingConvention;
		MemberFunctionAttribField attribField;
		uint16_t paramNum;
		uint32_t paramListTypeIndex;
		uint32_t thisAdjust;
	};

	struct LeafTypeArgList
	{
		uint32_t argNum;
		ByteBuffer typeIndices;
	};

	struct LeafTypeArray
	{
		uint32_t elementTypeIndex;
		uint32_t indexTypeIndex;
		NumericType size;
	};

	struct LeafTypeClassStructInterface
	{
		uint16_t elemNum;
		uint32_t fieldListTypeIndex;
		LeafPropertyField propertyField;
		uint32_t derivationListTypeIndex;
		uint32_t vShapeTypeIndex;
		NumericType sizeOf;
		ByteBuffer name;
		ByteBuffer uniqueName;
	};

	struct LeafTypeUnion
	{
		uint16_t count;
		LeafPropertyField propertyField;
		uint32_t fieldListTypeIndex;
		NumericType sizeOf;
		ByteBuffer name;
		ByteBuffer uniqueName;
	};

	enum class EVTShapeDesc : uint8_t
	{
		VTS_NEAR,
		VTS_FAR,
		VTS_THIN,
		VTS_OUTER,
		VTS_META,
		VTS_NEAR32,
		VTS_FAR32,
		VTS_UNUSED
	};

	struct LeafTypeVTShape
	{
		uint16_t count;
		EVTShapeDesc desc : 4;
	};

	struct LeafTypeBitfield
	{
		uint32_t typeIndex;
		uint8_t length;
		uint8_t position;
	};

	struct MethodListEntry
	{
		MemberAttribField attribField;
		uint16_t padding;
		uint32_t procedureTypeIndex;
		int32_t vTableOffset;
	};

	struct LeafTypeMethodList
	{
		uint32_t methodNum;
		ByteBuffer methods;
	};

	struct LeafTypeUDTSrcLine
	{
		uint32_t udtTypeIndex;
		uint32_t srcFileIndex;
		uint32_t lineNumber;
	};

	struct LeafTypeUDTModSrcLine
	{
		uint32_t udtTypeIndex;
		uint32_t srcFileIndex;
		uint32_t lineNumber; 
		uint16_t moduleIndex;
	};

	struct LeafRecord
	{
		ELeafType type;
		union
		{
			LeafTypeModifier modifier;
			LeafTypeEnum enum_;
			LeafTypeFieldList fieldList;
			LeafTypePointer pointer;
			LeafTypeProcedure procedure;
			LeafTypeMemberFunction memberFunction;
			LeafTypeArgList argList;
			LeafTypeArray array;
			LeafTypeClassStructInterface class_;
			LeafTypeClassStructInterface struct_;
			LeafTypeClassStructInterface interface_;
			LeafTypeUnion union_;
			LeafTypeVTShape vtShape;
			LeafTypeBitfield bitfield;
			LeafTypeMethodList methodList;
			LeafTypeUDTSrcLine udtSrcLine;
			LeafTypeVFTable vfTable;
		};
	};

	bool isClassStructOrInterface(const LeafRecord& leaf)
	{
		return
			leaf.type == ELeafType::LEAF_TYPE_CLASS ||
			leaf.type == ELeafType::LEAF_TYPE_STRUCTURE ||
			leaf.type == ELeafType::LEAF_TYPE_INTERFACE;
	}

	bool hasSize(const LeafRecord& leaf)
	{
		return
			leaf.type == ELeafType::LEAF_TYPE_ARRAY ||
			leaf.type == ELeafType::LEAF_TYPE_UNION ||
			leaf.type == ELeafType::LEAF_TYPE_CLASS ||
			leaf.type == ELeafType::LEAF_TYPE_STRUCTURE ||
			leaf.type == ELeafType::LEAF_TYPE_INTERFACE;
	}

	struct TypeRecord
	{
		uint16_t length;
		uint32_t index;
		LeafRecord leaf;
	};

	const char* pdbTpiStreamVersionToStr(EPDBTpiStreamVersion type)
	{
		switch (type)
		{
		case EPDBTpiStreamVersion::V40: return "v40";
		case EPDBTpiStreamVersion::V41: return "v41";
		case EPDBTpiStreamVersion::V50: return "v50";
		case EPDBTpiStreamVersion::V70: return "v70";
		case EPDBTpiStreamVersion::V80: return "v80";
		default: break;
		}
		return "Unknown";
	}

	MemberRecord readMemberRecordEmpty(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		MemberRecord leaf = {};
		outputByteOffset = 0;
		return leaf;
	}

	MemberRecord readMemberRecordUnknown(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		ALERT("Unknown Member Record Code View Type 0x%04X\n", (uint16_t)stream.read<ELeafType>());
		outputByteOffset = 0;
		return leaf;
	}

	NumericType readNumeric(const uint8_t* pBuffer, uint64_t length, uint64_t* pOutputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		NumericType leaf = {};
		uint16_t value = stream.readU16();
		if (value >= (uint16_t)ENumericType::NUM_SINT8)
		{
			leaf.type = (ENumericType)value;
			if (leaf.type < ENumericType::NUM_FLOAT32 ||
				(leaf.type > ENumericType::NUM_FLOAT128 && leaf.type < ENumericType::NUM_FLOAT48))
			{
				leaf.value = ByteBufferPool::get().add(stream.getBufferCurr(), (uint32_t)getNumericSize(leaf.type));
			}
			else
			{
				ALERT("Some numeric types aren't implemented yet. Requested Type = <0x%04X>", (uint16_t)leaf.type);
			}
		}
		else
		{
			leaf.type = ENumericType::NUM_INLINE16;
			leaf.value = ByteBufferPool::get().add((const uint8_t*)&value, sizeof(uint16_t));
		}
		if (pOutputByteOffset != nullptr)
			*pOutputByteOffset += getNumericSizeWithHeader(leaf.type);
		return leaf;
	}

	ByteBuffer readName(const uint8_t* pBuffer, uint64_t length, uint64_t* pOutputByteOffset)
	{
		size_t nameSize = strlen((const char*)pBuffer);
		ByteBuffer name = ByteBufferPool::get().add(pBuffer, (uint32_t)nameSize, true);
		if (pOutputByteOffset != nullptr)
			*pOutputByteOffset += nameSize + 1;
		return name;
	}

	MemberRecord readMemberRecordEnumerate(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();

		uint16_t attribRaw = stream.readU16();
		memcpy(&leaf.enumerate.attrib, &attribRaw, sizeof(uint16_t));

		uint64_t numericOffset = 0;
		leaf.enumerate.numeric = readNumeric(stream.getBufferCurr(), length, &numericOffset);
		stream.getReadOffset() += numericOffset;

		uint64_t nameOffset = 0;
		leaf.enumerate.name = readName(stream.getBufferCurr(), length, &nameOffset);
		outputByteOffset += stream.getReadOffset() + nameOffset;
		return leaf;
	}

	MemberRecord readMemberRecordNestedType(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		stream.readU16();
		leaf.nestedType.parentTypeIndex = stream.readU32();
		leaf.nestedType.name = readName(stream.getBufferCurr(), length, &outputByteOffset);
		outputByteOffset += stream.getReadOffset();
		return leaf;
	}

	MemberRecord readMemberRecordBaseClass(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.baseClass.attrib = stream.read<MemberAttribField>(); // reads 2 bytes from buffer
		// compiler inserts 2 bytes padding in struct here, but we don't read them from buffer
		leaf.baseClass.classTypeIndex = stream.readU32();                 // reads next 4 bytes from buffer

		uint64_t numericOffset = 0;
		leaf.baseClass.subObjectOffset = readNumeric(stream.getBufferCurr(), length, &numericOffset);
		outputByteOffset += stream.getReadOffset() + numericOffset;
		return leaf;
	}

	MemberRecord readMemberRecordMethod(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.method.overloads = stream.readU16();
		leaf.method.methodListTypeIndex = stream.readU32();
		leaf.method.name = readName(stream.getBufferCurr(), length, &outputByteOffset);
		outputByteOffset += stream.getReadOffset();
		return leaf;
	}

	MemberRecord readMemberRecordOneMethod(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();

		uint16_t attribRaw = stream.readU16();
		memcpy(&leaf.oneMethod.attrib, &attribRaw, sizeof(uint16_t));

		leaf.oneMethod.procedureTypeIndex = stream.readU32();
		if (isMemberVirtual(leaf.oneMethod.attrib.property))
			leaf.oneMethod.vTableOffset = stream.readS32();
		else
			leaf.oneMethod.vTableOffset = -1;

		uint64_t nameOffset = 0;
		leaf.oneMethod.name = readName(stream.getBufferCurr(), length, &nameOffset);
		outputByteOffset += stream.getReadOffset() + nameOffset;
		return leaf;
	}

	MemberRecord readMemberRecordBaseInterface(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();

		uint16_t attribRaw = stream.readU16();
		memcpy(&leaf.baseClass.attrib, &attribRaw, sizeof(uint16_t));

		leaf.baseClass.classTypeIndex = stream.readU32();

		uint64_t numericOffset = 0;
		leaf.baseClass.subObjectOffset = readNumeric(stream.getBufferCurr(), length, &numericOffset);
		outputByteOffset += stream.getReadOffset() + numericOffset;
		return leaf;
	}

	MemberRecord readMemberRecordVirtualBaseClass(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();

		uint16_t attribRaw = stream.readU16();
		memcpy(&leaf.virtualBaseClass.attribField, &attribRaw, sizeof(uint16_t));

		leaf.virtualBaseClass.directVirtualBaseClassTypeIndex = stream.readU32();
		leaf.virtualBaseClass.virtualBasePointerTypeIndex = stream.readU32();

		uint64_t numericOffset = 0;
		leaf.virtualBaseClass.vbPtrAddressOffset = readNumeric(stream.getBufferCurr(), length, &numericOffset);
		stream.getReadOffset() += numericOffset;
		numericOffset = 0;
		leaf.virtualBaseClass.vbPtrVTableOffset = readNumeric(stream.getBufferCurr(), length, &numericOffset);

		outputByteOffset += stream.getReadOffset() + numericOffset;
		return leaf;
	}

	MemberRecord readMemberRecordVirtualFunctionTable(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.vTablePtr.padding = stream.readU16();
		leaf.vTablePtr.pointerTypeIndex = stream.readU32();
		outputByteOffset += stream.getReadOffset();
		return leaf;
	}

	MemberRecord readMemberRecordStaticDataMember(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();

		uint16_t attribRaw = stream.readU16();
		memcpy(&leaf.staticDataMember.attribField, &attribRaw, sizeof(uint16_t));

		leaf.staticDataMember.typeIndexRecordField = stream.readU32();

		uint64_t nameOffset = 0;
		leaf.staticDataMember.name = readName(stream.getBufferCurr(), length, &nameOffset);
		outputByteOffset += stream.getReadOffset() + nameOffset;
		return leaf;
	}

	MemberRecord readMemberRecordDataMember(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();

		uint16_t attribRaw = stream.readU16();
		memcpy(&leaf.dataMember.attribField, &attribRaw, sizeof(uint16_t));

		leaf.dataMember.typeIndexRecordField = stream.readU32();

		uint64_t numericOffset = 0;
		leaf.dataMember.fieldOffset = readNumeric(stream.getBufferCurr(), length, &numericOffset);
		stream.getReadOffset() += numericOffset;

		uint64_t nameOffset = 0;
		leaf.dataMember.name = readName(stream.getBufferCurr(), length, &nameOffset);
		outputByteOffset += stream.getReadOffset() + nameOffset;
		return leaf;
	}

	MemberRecord readMemberRecordLeafIndex(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		BufferStreamReader stream(pBuffer, length);
		MemberRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.leafIndex.padding = stream.readU16();
		leaf.leafIndex.leafTypeIndex = stream.readU32();
		outputByteOffset += stream.getReadOffset();
		return leaf;
	}

	MemberRecord readMemberRecord(const uint8_t* pBuffer, uint64_t length, uint64_t& outputByteOffset)
	{
		ELeafType cvType = *(ELeafType*)pBuffer;
		switch (cvType)
		{
		case ELeafType::LEAF_TYPE_BCLASS:    return readMemberRecordBaseClass(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_METHOD:    return readMemberRecordMethod(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_NESTTYPE:  return readMemberRecordNestedType(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_ONEMETHOD: return readMemberRecordOneMethod(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_ENUMERATE: return readMemberRecordEnumerate(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_BINTERFACE:return readMemberRecordBaseInterface(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_VBCLASS:   return readMemberRecordVirtualBaseClass(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_IVBCLASS:  return readMemberRecordVirtualBaseClass(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_VFUNCTAB:  return readMemberRecordVirtualFunctionTable(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_STMEMBER:  return readMemberRecordStaticDataMember(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_MEMBER:    return readMemberRecordDataMember(pBuffer, length, outputByteOffset);
		case ELeafType::LEAF_TYPE_INDEX:     return readMemberRecordLeafIndex(pBuffer, length, outputByteOffset);
		default:                             return readMemberRecordUnknown(pBuffer, length, outputByteOffset);
		}
	}

	LeafRecord readLeafRecordEmpty(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		LeafRecord leaf = {};
		return leaf;
	}

	LeafRecord readLeafRecordUnknown(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		ALERT("Unknown Leaf Record Code View Type 0x%04X\n", stream.read<ELeafType>());
		return leaf;
	}

	LeafRecord readLeafRecordPointer(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.pointer.underlyingType = stream.readU32();
		leaf.pointer.attrib = stream.read<PointerAttribField>();
		return leaf;
	}

	LeafRecord readLeafRecordModifier(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.modifier.typeIndex = stream.readU32();
		leaf.modifier.attrib = stream.read<ModifierAttributeField>();
		return leaf;
	}

	LeafRecord readLeafRecordFieldList(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.fieldList.fieldNum = 0;
		const uint8_t* pFieldsBuffer = stream.getBufferCurr();
		uint64_t totalByteOffset = 0;
		uint64_t currentLength = length - stream.getReadOffset();

		static TArray<MemberRecord> staticBuffer;
		while (totalByteOffset < currentLength)
		{
			while (*(pFieldsBuffer + totalByteOffset) > 0xf0)
				totalByteOffset++;

			if (totalByteOffset >= currentLength) break;

			uint64_t byteOffset = 0;
			MemberRecord memberRecord = readMemberRecord(pFieldsBuffer + totalByteOffset, length, byteOffset);
			if (byteOffset == 0) break;
			totalByteOffset += byteOffset;
			leaf.fieldList.fieldNum += 1;
			staticBuffer.add(memberRecord);
		}
		leaf.fieldList.members = MemberRecordTable::get().add(staticBuffer.getData(), (uint32_t)staticBuffer.getNum());
		staticBuffer.reset();
		return leaf;
	}

	LeafRecord readLeafRecordEnum(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.enum_.enumNum = stream.readU16();
		leaf.enum_.propertyField = stream.read<LeafPropertyField>();
		leaf.enum_.underlyingType = stream.readU32();
		leaf.enum_.fieldListTypeIndex = stream.readU32();
		leaf.enum_.name = readName(stream.getBufferCurr(), length, nullptr);
		if (leaf.enum_.propertyField.bHasUniqueName)
		{
			leaf.enum_.uniqueName = readName(stream.getBufferCurr() + leaf.enum_.name.len() + 1, length, nullptr);
		}
		if (!leaf.enum_.propertyField.bIsForwardRef)
		{
			if (leaf.enum_.propertyField.bHasUniqueName)
				UDTTable::get().add(leaf.enum_.uniqueName, typeIndex);
			else
				UDTTable::get().add(leaf.enum_.name, typeIndex);
		}
		return leaf;
	}

	LeafRecord readLeafRecordProcedure(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.procedure.returnTypeIndex = stream.readU32();
		leaf.procedure.callingConvention = stream.read<ECallingConvention>(); stream.readU8();
		leaf.procedure.paramNum = stream.readU16();
		leaf.procedure.argListTypeIndex = stream.readU32(); stream.readU16();
		return leaf;
	}

	LeafRecord readLeafRecordMemberFunction(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.memberFunction.returnTypeIndex = stream.readU32();
		leaf.memberFunction.classTypeIndex = stream.readU32();
		leaf.memberFunction.thisTypeIndex = stream.readU32();
		leaf.memberFunction.callingConvention = stream.read<ECallingConvention>();
		leaf.memberFunction.attribField = stream.read<MemberFunctionAttribField>();
		leaf.memberFunction.paramNum = stream.readU16();
		leaf.memberFunction.paramListTypeIndex = stream.readU32();
		leaf.memberFunction.thisAdjust = stream.readU32();
		return leaf;
	}

	LeafRecord readLeafRecordArgList(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.argList.argNum = stream.readU32();
		if (leaf.argList.argNum > 0)
		{
			static TArray<uint32_t> indices;
			for (uint32_t index = 0; index < leaf.argList.argNum; ++index)
			{
				indices.add(stream.readU32());
			}
			leaf.argList.typeIndices = ByteBufferPool::get().tAdd(indices.getData(), (uint32_t)indices.getNum());
			indices.reset();
		}
		return leaf;
	}

	LeafRecord readLeafRecordLabel(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("Label");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordArray(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.array.elementTypeIndex = stream.readU32();
		leaf.array.indexTypeIndex = stream.readU32();
		leaf.array.size = readNumeric(stream.getBufferCurr(), length, nullptr);
		return leaf;
	}

	LeafRecord readLeafRecordClass(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.class_.elemNum = stream.readU16();
		leaf.class_.propertyField = stream.read<LeafPropertyField>();
		leaf.class_.fieldListTypeIndex = stream.readU32();
		leaf.class_.derivationListTypeIndex = stream.readU32();
		leaf.class_.vShapeTypeIndex = stream.readU32();
		leaf.class_.sizeOf = readNumeric(stream.getBufferCurr(), length, nullptr);
		leaf.class_.name = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.class_.sizeOf.type), length, nullptr);
		if (leaf.class_.propertyField.bHasUniqueName)
		{
			leaf.class_.uniqueName = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.class_.sizeOf.type) + leaf.class_.name.len() + 1, length, nullptr);
		}
		if (!leaf.class_.propertyField.bIsForwardRef)
		{
			if (leaf.class_.propertyField.bHasUniqueName)
				UDTTable::get().add(leaf.class_.uniqueName, typeIndex);
			else
				UDTTable::get().add(leaf.class_.name, typeIndex);
		}
		return leaf;
	}

	LeafRecord readLeafRecordStruct(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.struct_.elemNum = stream.readU16();
		leaf.struct_.propertyField = stream.read<LeafPropertyField>();
		leaf.struct_.fieldListTypeIndex = stream.readU32();
		leaf.struct_.derivationListTypeIndex = stream.readU32();
		leaf.struct_.vShapeTypeIndex = stream.readU32();
		leaf.struct_.sizeOf = readNumeric(stream.getBufferCurr(), length, nullptr);
		leaf.struct_.name = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.struct_.sizeOf.type), length, nullptr);
		if (leaf.struct_.propertyField.bHasUniqueName)
		{
			leaf.struct_.uniqueName = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.struct_.sizeOf.type) + leaf.struct_.name.len() + 1, length, nullptr);
		}
		if (!leaf.struct_.propertyField.bIsForwardRef)
		{
			if (leaf.struct_.propertyField.bHasUniqueName)
				UDTTable::get().add(leaf.struct_.uniqueName, typeIndex);
			else
				UDTTable::get().add(leaf.struct_.name, typeIndex);
		}
		return leaf;
	}

	LeafRecord readLeafRecordInterface(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.interface_.elemNum = stream.readU16();
		leaf.interface_.propertyField = stream.read<LeafPropertyField>();
		leaf.interface_.fieldListTypeIndex = stream.readU32();
		leaf.interface_.derivationListTypeIndex = stream.readU32();
		leaf.interface_.vShapeTypeIndex = stream.readU32();
		leaf.interface_.sizeOf = readNumeric(stream.getBufferCurr(), length, nullptr);
		leaf.interface_.name = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.interface_.sizeOf.type), length, nullptr);
		if (leaf.interface_.propertyField.bHasUniqueName)
		{
			leaf.interface_.uniqueName = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.interface_.sizeOf.type) + leaf.interface_.name.len() + 1, length, nullptr);
		}
		if (!leaf.interface_.propertyField.bIsForwardRef)
		{
			if (leaf.interface_.propertyField.bHasUniqueName)
				UDTTable::get().add(leaf.interface_.uniqueName, typeIndex);
			else
				UDTTable::get().add(leaf.interface_.name, typeIndex);
		}
		return leaf;
	}

	LeafRecord readLeafRecordUnion(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.union_.count = stream.readU16();
		leaf.union_.propertyField = stream.read<LeafPropertyField>();
		leaf.union_.fieldListTypeIndex = stream.readU32();
		leaf.union_.sizeOf = readNumeric(stream.getBufferCurr(), length, nullptr);
		leaf.union_.name = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.union_.sizeOf.type), length, nullptr);
		if (leaf.union_.propertyField.bHasUniqueName)
		{
			leaf.union_.uniqueName = readName(stream.getBufferCurr() + getNumericSizeWithHeader(leaf.union_.sizeOf.type) + leaf.union_.name.len() + 1, length, nullptr);
		}
		if (!leaf.union_.propertyField.bIsForwardRef)
		{
			if (leaf.union_.propertyField.bHasUniqueName)
				UDTTable::get().add(leaf.union_.uniqueName, typeIndex);
			else
				UDTTable::get().add(leaf.union_.name, typeIndex);
		}
		return leaf;
	}

	LeafRecord readLeafRecordTypeServer2(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("Type Server 2");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordVirtualFunctionTable(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.vfTable.completeClass = stream.readU32();
		leaf.vfTable.overriddenVFTable = stream.readU32();
		leaf.vfTable.vfPtrOffset = stream.readU32();
		leaf.vfTable.namesLen = stream.readU32();
		return leaf;
	}

	LeafRecord readLeafRecordVirtualFunctionTableShape(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.vtShape.count = stream.readU16();
		leaf.vtShape.desc = stream.read<EVTShapeDesc>();
		return leaf;
	}

	LeafRecord readLeafRecordBitfield(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.bitfield.typeIndex = stream.readU32();
		leaf.bitfield.length = stream.readU8();
		leaf.bitfield.position = stream.readU8();
		return leaf;
	}

	LeafRecord readLeafRecordMethodList(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.methodList.methodNum = 0;

		static TArray<MethodListEntry> staticBuffer;
		while (stream.getReadOffset() < length)
		{
			MethodListEntry method;

			uint16_t attribRaw = stream.readU16();
			memcpy(&method.attribField, &attribRaw, sizeof(uint16_t));

			method.padding = stream.readU16();
			method.procedureTypeIndex = stream.readU32();
			if (isMemberVirtual(method.attribField.property))
				method.vTableOffset = stream.readS32();
			else
				method.vTableOffset = -1;

			staticBuffer.add(method);
			leaf.methodList.methodNum += 1;
		}

		leaf.methodList.methods = ByteBufferPool::get().tAdd(staticBuffer.getData(), (uint32_t)staticBuffer.getNum());
		staticBuffer.reset();
		return leaf;
	}

	LeafRecord readLeafRecordPrecompiledType(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("Precompiled Types");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordEndPrecompiledType(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("End Precompiled Types");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordFunctionId(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("Function ID");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordMemberFunctionId(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("Member Function ID");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordBuildInfo(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("Build Info");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordSubstringList(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		NOT_IMPLEMENTED_LEAF_TYPE("Substring List");
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		return leaf;
	}

	LeafRecord readLeafRecordStringId(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		stream.readU32();
		const char* str = (const char*)stream.getBufferCurr();
		IPIStringTable::get().add(typeIndex, String(str));
		return leaf;
	}

	LeafRecord readLeafRecordUDTSourceLine(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.udtSrcLine.udtTypeIndex = stream.readU32();
		leaf.udtSrcLine.srcFileIndex = stream.readU32();
		leaf.udtSrcLine.lineNumber = stream.readU32();
		UDTSrcLineTable::get().add(
			leaf.udtSrcLine.udtTypeIndex,
			leaf.udtSrcLine.srcFileIndex,
			leaf.udtSrcLine.lineNumber);
		return leaf;
	}

	LeafRecord readLeafRecordUDTModSourceLine(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		BufferStreamReader stream(pBuffer, length);
		LeafRecord leaf = {};
		leaf.type = stream.read<ELeafType>();
		leaf.udtSrcLine.udtTypeIndex = stream.readU32();
		leaf.udtSrcLine.srcFileIndex = stream.readU32();
		leaf.udtSrcLine.lineNumber = stream.readU32();
		stream.readU16();
		UDTSrcLineTable::get().add(
			leaf.udtSrcLine.udtTypeIndex,
			leaf.udtSrcLine.srcFileIndex,
			leaf.udtSrcLine.lineNumber);
		return leaf;
	}

	LeafRecord readLeafRecord(const uint8_t* pBuffer, uint64_t length, uint32_t typeIndex)
	{
		ELeafType cvType = *(ELeafType*)pBuffer;
		switch (cvType)
		{
		case ELeafType::LEAF_TYPE_POINTER:          return readLeafRecordPointer(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_MODIFIER:         return readLeafRecordModifier(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_PROCEDURE:        return readLeafRecordProcedure(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_MFUNCTION:        return readLeafRecordMemberFunction(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_LABEL:            return readLeafRecordLabel(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_ARGLIST:          return readLeafRecordArgList(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_FIELDLIST:        return readLeafRecordFieldList(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_ARRAY:            return readLeafRecordArray(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_ENUM:             return readLeafRecordEnum(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_CLASS:            return readLeafRecordClass(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_STRUCTURE:        return readLeafRecordStruct(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_INTERFACE:        return readLeafRecordInterface(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_UNION:            return readLeafRecordUnion(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_TYPESERVER2:      return readLeafRecordTypeServer2(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_VFTABLE:          return readLeafRecordVirtualFunctionTable(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_VTSHAPE:          return readLeafRecordVirtualFunctionTableShape(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_BITFIELD:         return readLeafRecordBitfield(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_METHODLIST:       return readLeafRecordMethodList(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_PRECOMP:          return readLeafRecordPrecompiledType(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_ENDPRECOMP:       return readLeafRecordEndPrecompiledType(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_FUNC_ID:          return readLeafRecordFunctionId(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_MFUNC_ID:         return readLeafRecordMemberFunctionId(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_BUILDINFO:        return readLeafRecordBuildInfo(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_SUBSTR_LIST:      return readLeafRecordSubstringList(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_STRING_ID:        return readLeafRecordStringId(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_UDT_SRC_LINE:     return readLeafRecordUDTSourceLine(pBuffer, length, typeIndex);
		case ELeafType::LEAF_TYPE_UDT_MOD_SRC_LINE: return readLeafRecordUDTModSourceLine(pBuffer, length, typeIndex);
		default:                                    return readLeafRecordUnknown(pBuffer, length, typeIndex);
		}
	}

	void printTPIHeader(PDBTpiStreamHeader header)
	{
		LOG("TPI:\n"
			"Version: %s\n"
			"HeaderSize: %u\n"
			"TypeIndexBegin: %u\n"
			"TypeIndexEnd: %u\n"
			"TypeRecordBytes: %u\n"
			"HashStreamIndex: %u\n"
			"HashAuxStreamIndex: %u\n"
			"HashKeySize: %u\n"
			"NumHashBuckets: %u\n"
			"HashValueBufferOffset: %d\n"
			"HashValueBufferLength: %u\n"
			"IndexOffsetBufferOffset: %d\n"
			"IndexOffsetBufferLength: %u\n"
			"HashAdjBufferOffset: %d\n"
			"HashAdjBufferLength: %u\n",
			pdbTpiStreamVersionToStr(header.version),
			header.headerSize,
			header.typeIndexBegin,
			header.typeIndexEnd,
			header.typeRecordBytes,
			header.hashStreamIndex,
			header.hashAuxStreamIndex,
			header.hashKeySize,
			header.numHashBuckets,
			header.hashValueBufferOffset,
			header.hashValueBufferLength,
			header.indexOffsetBufferOffset,
			header.indexOffsetBufferLength,
			header.hashAdjBufferOffset,
			header.hashAdjBufferLength
		);
	}
	EPDBResult processDBIStream(const uint8_t* pInputData, uint64_t dataSize, MSFSuperBlock superBlock, MSFStreamBlock streamBlock, MSFStreamDirectory streamDirectory)
	{
		struct DBIStreamHeader
		{
			int32_t versionSignature;
			uint32_t versionHeader;
			uint32_t age;
			uint16_t globalStreamIndex;
			uint16_t buildNumber;
			uint16_t publicStreamIndex;
			uint16_t pdbDllVersion;
			uint16_t symRecordStream;
			uint16_t pdbDllRbld;
			int32_t modInfoSize;
			int32_t sectionContributionSize;
			int32_t sectionMapSize;
			int32_t sourceInfoSize;
			int32_t typeServerMapSize;
			uint32_t mfcTypeServerIndex;
			int32_t optionalDbgHeaderSize;
			int32_t ecSubstreamSize;
			uint16_t flags;
			uint16_t machine;
			uint32_t padding;
		};

		PDBStreamBlock block = readBlocks(pInputData, dataSize, superBlock, streamBlock);
		DBIStreamHeader dbiHeader = {};
		dbiHeader.versionSignature = block.readS32();
		dbiHeader.versionHeader = block.readU32();
		dbiHeader.age = block.readU32();
		dbiHeader.globalStreamIndex = block.readU16();
		dbiHeader.buildNumber = block.readU16();
		dbiHeader.publicStreamIndex = block.readU16();
		dbiHeader.pdbDllVersion = block.readU16();
		dbiHeader.symRecordStream = block.readU16();
		dbiHeader.pdbDllRbld = block.readU16();
		dbiHeader.modInfoSize = block.readS32();
		dbiHeader.sectionContributionSize = block.readS32();
		dbiHeader.sectionMapSize = block.readS32();
		dbiHeader.sourceInfoSize = block.readS32();
		dbiHeader.typeServerMapSize = block.readS32();
		dbiHeader.mfcTypeServerIndex = block.readU32();
		dbiHeader.optionalDbgHeaderSize = block.readS32();
		dbiHeader.ecSubstreamSize = block.readS32();
		dbiHeader.flags = block.readU16();
		dbiHeader.machine = block.readU16();
		dbiHeader.padding = block.readS32();

		LOG("DBI:\n"
			"VersionSignature: %d\n"
			"VersionHeader: %u\n"
			"Age: %u\n"
			"GlobalStreamIndex: %u\n"
			"BuildNumber: %u\n"
			"PublicStreamIndex: %u\n"
			"PdbDllVersion: %u\n"
			"SymRecordStream: %u\n"
			"PdbDllRbld: %u\n"
			"ModInfoSize: %d\n"
			"SectionContributionSize: %d\n"
			"SectionMapSize: %d\n"
			"SourceInfoSize: %d\n"
			"TypeServerMapSize: %d\n"
			"MFCTypeServerIndex: %u\n"
			"OptionalDbgHeaderSize: %d\n"
			"ECSubstreamSize: %d\n"
			"Flags: %u\n"
			"Machine: 0x%X\n"
			"Padding: %d\n",
			dbiHeader.versionSignature,
			dbiHeader.versionHeader,
			dbiHeader.age,
			dbiHeader.globalStreamIndex,
			dbiHeader.buildNumber,
			dbiHeader.publicStreamIndex,
			dbiHeader.pdbDllVersion,
			dbiHeader.symRecordStream,
			dbiHeader.pdbDllRbld,
			dbiHeader.modInfoSize,
			dbiHeader.sectionContributionSize,
			dbiHeader.sectionMapSize,
			dbiHeader.sourceInfoSize,
			dbiHeader.typeServerMapSize,
			dbiHeader.mfcTypeServerIndex,
			dbiHeader.optionalDbgHeaderSize,
			dbiHeader.ecSubstreamSize,
			dbiHeader.flags,
			dbiHeader.machine,
			dbiHeader.padding
		);

		pointerSize = 4;

		switch (dbiHeader.machine)
		{
		case MACHINE_TYPE_RISCV128:
			pointerSize = 16;
			break;
		case MACHINE_TYPE_LOONGARCH64:
		case MACHINE_TYPE_RISCV64:
		case MACHINE_TYPE_AXP64:
		case MACHINE_TYPE_ARM64:
		case MACHINE_TYPE_AMD64:
		case MACHINE_TYPE_UNKNOWN:
		default:
			pointerSize = 8;
			break;
		}

		if (dbiHeader.machine == MACHINE_TYPE_UNKNOWN)
		{
			LOG("Unknown machine type. Will process as x64");
		}

		return EPDBResult::RESULT_SUCCESS;
	}

	EPDBResult processIPIStream(const uint8_t* pInputData, uint64_t dataSize, MSFSuperBlock superBlock, MSFStreamBlock streamBlock, MSFStreamDirectory streamDirectory)
	{
		if (streamBlock.blockCount == 0)
			return EPDBResult::RESULT_SUCCESS;

		PDBStreamBlock block = readBlocks(pInputData, dataSize, superBlock, streamBlock);
		PDBTpiStreamHeader header = {};
		header.version = (EPDBTpiStreamVersion)block.readU32();
		header.headerSize = block.readU32();
		header.typeIndexBegin = block.readU32();
		header.typeIndexEnd = block.readU32();
		header.typeRecordBytes = block.readU32();
		header.hashStreamIndex = block.readU16();
		header.hashAuxStreamIndex = block.readU16();
		header.hashKeySize = block.readU32();
		header.numHashBuckets = block.readU32();
		header.hashValueBufferOffset = block.readS32();
		header.hashValueBufferLength = block.readU32();
		header.indexOffsetBufferOffset = block.readS32();
		header.indexOffsetBufferLength = block.readU32();
		header.hashAdjBufferOffset = block.readS32();
		header.hashAdjBufferLength = block.readU32();

		uint32_t totalTypeCount = header.typeIndexEnd - header.typeIndexBegin;
		for (uint32_t index = 0; index < totalTypeCount; ++index)
		{
			uint32_t typeIndex = header.typeIndexBegin + index;
			uint16_t length = block.readU16();
			readLeafRecord(block.getBufferCurr(), length, typeIndex);
			block.getReadOffset() += length;
		}

		freeBlocks(block.getBlocks());
		return EPDBResult::RESULT_SUCCESS;
	}

	EPDBResult processPDBStream(const uint8_t* pInputData, uint64_t dataSize, MSFSuperBlock superBlock, MSFStreamBlock streamBlock, MSFStreamDirectory streamDirectory)
	{
		PDBStreamBlock block = readBlocks(pInputData, dataSize, superBlock, streamBlock);

		PDBStreamHeader header = {};
		header.version = (EPDBStreamVersion)block.readU32();
		header.signature = block.readU32();
		header.age = block.readU32();
		block.readBuffer(header.guid, sizeof(PDBStreamHeader::guid));

		uint32_t namesMapByteSize = block.readU32();
		const uint8_t* namesMapBuffer = block.getBufferCurr();
		block.getReadOffset() += namesMapByteSize;

		uint32_t numWords = block.readU32();
		uint32_t numBuckets = block.readU32();

		uint32_t presentWordCount = block.readU32();
		block.getReadOffset() += presentWordCount * sizeof(uint32_t);

		uint32_t deletedWordCount = block.readU32();
		block.getReadOffset() += deletedWordCount * sizeof(uint32_t);

		int32_t namesStreamIndex = -1;
		for (uint32_t i = 0; i < numBuckets; ++i)
		{
			uint32_t nameOff = block.readU32();
			uint32_t streamIdx = block.readU32();
			if (nameOff < namesMapByteSize)
			{
				const char* entryName = (const char*)(namesMapBuffer + nameOff);
				if (strcmp(entryName, "/names") == 0)
				{
					namesStreamIndex = (int32_t)streamIdx;
				}
			}
		}

		freeBlocks(block.getBlocks());

		if (namesStreamIndex >= 0 && (uint32_t)namesStreamIndex < streamDirectory.numStreams)
		{
			PDBStreamBlock namesBlock = readBlocks(pInputData, dataSize, superBlock, streamDirectory.pStreamBlocks[namesStreamIndex]);

			uint32_t nameSig = namesBlock.readU32();
			uint32_t nameVer = namesBlock.readU32();
			uint32_t bufferSize = namesBlock.readU32();

			IPIStringTable::get().setNamesBuffer(namesBlock.getBufferCurr(), bufferSize);

			freeBlocks(namesBlock.getBlocks());
		}
		else
		{
			LOG("Could not find /names stream");
		}

		return EPDBResult::RESULT_SUCCESS;
	}


	EPDBResult processTPIStream(const uint8_t* pInputData, uint64_t dataSize, MSFSuperBlock superBlock, MSFStreamBlock streamBlock, MSFStreamDirectory streamDirectory, TArray<TypeRecord>& typeRecords)
	{
		PDBStreamBlock block = readBlocks(pInputData, dataSize, superBlock, streamBlock);
		PDBTpiStreamHeader header = {};
		header.version = (EPDBTpiStreamVersion)block.readU32();
		header.headerSize = block.readU32();
		header.typeIndexBegin = block.readU32();
		header.typeIndexEnd = block.readU32();
		header.typeRecordBytes = block.readU32();
		header.hashStreamIndex = block.readU16();
		header.hashAuxStreamIndex = block.readU16();
		header.hashKeySize = block.readU32();
		header.numHashBuckets = block.readU32();
		header.hashValueBufferOffset = block.readS32();
		header.hashValueBufferLength = block.readU32();
		header.indexOffsetBufferOffset = block.readS32();
		header.indexOffsetBufferLength = block.readU32();
		header.hashAdjBufferOffset = block.readS32();
		header.hashAdjBufferLength = block.readU32();

		if (header.headerSize != sizeof(PDBTpiStreamHeader))
		{
			printTPIHeader(header);
			return EPDBResult::RESULT_FAIL_TPI_HEADER_SIZE;
		}

		uint32_t totalTypeCount = header.typeIndexEnd - header.typeIndexBegin;
		uint32_t totalTypeRecordSizeInBytes = totalTypeCount * header.hashKeySize;

		if (header.hashValueBufferLength != totalTypeRecordSizeInBytes)
		{
			printTPIHeader(header);
			return EPDBResult::RESULT_FAIL_TPI_HEADER_HASH_BUFFER_LEN;
		}

		typeRecords.resize(totalTypeCount);

		for (uint32_t index = 0; index < totalTypeCount; ++index)
		{
			TypeRecord type = {};
			type.index = 0x1000 + index;
			type.length = block.readU16();
			type.leaf = readLeafRecord(block.getBufferCurr(), type.length, type.index);
			block.getReadOffset() += type.length;
			typeRecords.add(type);
		}

		freeBlocks(block.getBlocks());
		printTPIHeader(header);
		return EPDBResult::RESULT_SUCCESS;
	}

	EPDBResult processStreams(const uint8_t* pInputData, uint64_t dataSize, MSFSuperBlock superBlock, MSFStreamDirectory streamDirectory, TArray<TypeRecord>& typeRecords)
	{
		EPDBResult result = EPDBResult::RESULT_SUCCESS;

		if ((result = processPDBStream(pInputData, dataSize, superBlock, streamDirectory.pStreamBlocks[1], streamDirectory)) != EPDBResult::RESULT_SUCCESS)
			return result;

		if ((result = processDBIStream(pInputData, dataSize, superBlock, streamDirectory.pStreamBlocks[3], streamDirectory)) != EPDBResult::RESULT_SUCCESS)
			return result;

		if (streamDirectory.numStreams > 4 && streamDirectory.pStreamBlocks[4].blockCount > 0)
		{
			if ((result = processIPIStream(pInputData, dataSize, superBlock, streamDirectory.pStreamBlocks[4], streamDirectory)) != EPDBResult::RESULT_SUCCESS)
				return result;
		}

		if ((result = processTPIStream(pInputData, dataSize, superBlock, streamDirectory.pStreamBlocks[2], streamDirectory, typeRecords)) != EPDBResult::RESULT_SUCCESS)
			return result;

		return result;
	}

	EPDBResult processPDB(const uint8_t* pInputData, uint64_t dataSize, TArray<TypeRecord>& typeRecords)
	{
		MSFSuperBlock superBlock = {};
		EPDBResult result = EPDBResult::RESULT_SUCCESS;
		if ((result = readMSFSuperBlock(pInputData, dataSize, superBlock)) != EPDBResult::RESULT_SUCCESS)
		{
			return result;
		}
		MSFStreamDirectory streamDirectory = {};
		if ((result = readMSFStreamDirectory(pInputData, dataSize, superBlock, streamDirectory)) != EPDBResult::RESULT_SUCCESS)
		{
			return result;
		}
		if ((result = processStreams(pInputData, dataSize, superBlock, streamDirectory, typeRecords)) != EPDBResult::RESULT_SUCCESS)
		{
			return result;
		}
		return result;
	}

	void outputTypeSizeReport(const char* pOutputFileName, TArray<TypeRecord>& typeRecords)
	{
		WriteBuffer writer(1024 * 1024 * 200);
		writer.append("Type\tName\tSize\n");
		for (uint64_t index = 0; index < typeRecords.getNum(); ++index)
		{
			const TypeRecord& record = typeRecords[index];
			if (record.leaf.type == ELeafType::LEAF_TYPE_CLASS && record.leaf.class_.sizeOf() > 0)
			{
				writer.append("class\t%s\t%u\n", record.leaf.class_.name(), record.leaf.class_.sizeOf());
			}
			else if (record.leaf.type == ELeafType::LEAF_TYPE_STRUCTURE && record.leaf.struct_.sizeOf() > 0)
			{
				writer.append("class\t%s\t%u\n", record.leaf.struct_.name(), record.leaf.struct_.sizeOf());
			}
			else if (record.leaf.type == ELeafType::LEAF_TYPE_INTERFACE && record.leaf.interface_.sizeOf() > 0)
			{
				writer.append("interface\t%s\t%u\n", record.leaf.interface_.name(), record.leaf.interface_.sizeOf());
			}
			else if (record.leaf.type == ELeafType::LEAF_TYPE_UNION && record.leaf.union_.sizeOf() > 0)
			{
				writer.append("union\t%s\t%u\n", record.leaf.union_.name(), record.leaf.union_.sizeOf());
			}
		}
		writer.flush(pOutputFileName, "-TypeSize", "txt");
	}

	bool getLeafRecordByIndex(const TArray<TypeRecord>& typeRecords, uint32_t typeIndex, TypeRecord& output)
	{
		if (typeIndex >= 0x1000)
		{
			uint32_t realIndex = typeIndex - 0x1000;
			output = typeRecords[realIndex];
			ALWAYS_ASSERT(output.index == typeIndex, "Requested type index (0x%08X) doesn't match record (0x%08X).", output.index, typeIndex);
			return true;
		}
		return false;
	}

	struct TypeInfo
	{
		uint32_t size;
		ByteBuffer typeName;
		uint32_t arraySize;
		const char* pDirectName;
		bool bIsPtr;
		EPointerMode ptrMode;
	};

	TypeInfo getTypeInfo(const TArray<TypeRecord>& typeRecords, uint32_t typeIndex)
	{
		if (typeIndex >= 0x1000)
		{
			TypeRecord record = {};
			if (getLeafRecordByIndex(typeRecords, typeIndex, record))
			{
				if (isClassStructOrInterface(record.leaf))
				{
					if (record.leaf.class_.propertyField.bIsForwardRef)
					{
						uint32_t resolvedIndex = UDTTable::get().getTypeIndex(record.leaf.class_.uniqueName);
						return getTypeInfo(typeRecords, resolvedIndex);
					}
					else
					{
						return { record.leaf.class_.sizeOf(), record.leaf.class_.name, 0, nullptr, false };
					}
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_UNION)
				{
					if (record.leaf.union_.propertyField.bIsForwardRef)
					{
						uint32_t resolvedIndex = UDTTable::get().getTypeIndex(record.leaf.union_.uniqueName);
						return getTypeInfo(typeRecords, resolvedIndex);
					}
					else
					{
						return { record.leaf.union_.sizeOf(), record.leaf.union_.name, 0, nullptr, false };
					}
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_ARRAY)
				{
					TypeInfo info = getTypeInfo(typeRecords, record.leaf.array.elementTypeIndex);
					return { record.leaf.array.size(), info.typeName, info.size > 0 ? record.leaf.array.size() / info.size : record.leaf.array.size(), info.pDirectName, false };
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_POINTER)
				{
					TypeInfo pointerTypeInfo = getTypeInfo(typeRecords, record.leaf.pointer.underlyingType);
					return { (uint32_t)pointerSize, pointerTypeInfo.typeName, 0, pointerTypeInfo.pDirectName, true, record.leaf.pointer.attrib.mode };
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_ENUM)
				{
					return getTypeInfo(typeRecords, record.leaf.enum_.underlyingType);
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_MODIFIER)
				{
					return getTypeInfo(typeRecords, record.leaf.modifier.typeIndex);
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_BITFIELD)
				{
					return getTypeInfo(typeRecords, record.leaf.bitfield.typeIndex);
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_PROCEDURE)
				{
					return { 0, ByteBuffer(), 0, "function", false };
				}
				else if (record.leaf.type == ELeafType::LEAF_TYPE_MFUNCTION)
				{
					TypeInfo info = getTypeInfo(typeRecords, record.leaf.memberFunction.classTypeIndex);
					return { 0, ByteBuffer(), 0, TempFmtByteBuffer("member function -> %s", info.typeName()), false };
				}
				ALERT("TypeInfo for 0x%08X type index is not implemented", typeIndex);
			}
			else
			{
				ALERT("Leaf Record couldn't be found for 0x%08X type index", typeIndex);
			}
		}
		else
		{
			size_t size = getPrimitiveSizeType(typeIndex);
			const char* pName = getPrimitiveName(typeIndex);
			bool bIsPtr = getPrimitiveTypeIsPtr(typeIndex);
			return { (uint32_t)size, ByteBuffer(), 0, pName, bIsPtr };
		}
		ALERT("Type index 0x%08X not found", typeIndex);
		return {};
	}

	void writeFieldList(WriteBuffer& writer, TArray<TypeRecord>& typeRecords, uint32_t fieldListTypeIndex, bool& bIsFirstElement, const char* pBaseClass = nullptr)
	{
		TypeRecord record = {};
		if (getLeafRecordByIndex(typeRecords, fieldListTypeIndex, record) && record.leaf.type == ELeafType::LEAF_TYPE_FIELDLIST)
		{
			uint16_t num = record.leaf.fieldList.fieldNum;
			for (uint16_t index = 0; index < num; ++index)
			{
				const MemberRecord& member = record.leaf.fieldList.members[index];
				if (member.type == ELeafType::LEAF_TYPE_MEMBER)
				{
					TypeInfo info = getTypeInfo(typeRecords, member.dataMember.typeIndexRecordField);
					const char* pTypeName = info.size > 0 ? info.typeName() : "undefined type";
					if (info.pDirectName != nullptr)
						pTypeName = info.pDirectName;

					const char* pPointerAppend = "";
					if (info.bIsPtr)
					{
						if (info.ptrMode == EPointerMode::MODE_LVALUE_REF) pPointerAppend = "&";
						else if (info.ptrMode == EPointerMode::MODE_RVALUE_REF) pPointerAppend = "&&";
						else pPointerAppend = "*";
					}

					const char* pAccessAppend = "none";
					switch (member.dataMember.attribField.access)
					{
					case EMemberAccessType::ACCESS_PRIVATE: pAccessAppend = "private"; break;
					case EMemberAccessType::ACCESS_PROTECTED: pAccessAppend = "protected"; break;
					case EMemberAccessType::ACCESS_PUBLIC: pAccessAppend = "public"; break;
					case EMemberAccessType::ACCESS_NO_PROTECTION: break;
					}

					if (!bIsFirstElement)
					{
						writer.append(",\n");
					}
					writer.append("\t\t\t{ \"name\": \"%s\", \"offset\": %u, \"size\": %u, \"type\": \"%s%s%s\", \"access\":\"%s\", \"baseClass\":\"%s\" }",
						member.dataMember.name(),
						member.dataMember.fieldOffset(),
						info.size,
						pTypeName,
						pPointerAppend,
						(info.arraySize > 0 ? TempFmtByteBuffer("[%u]", info.arraySize) : ""),
						pAccessAppend,
						(pBaseClass != nullptr) ? pBaseClass : ""
					);
					bIsFirstElement = false;
				}
				else if (member.type == ELeafType::LEAF_TYPE_BCLASS)
				{
					TypeRecord baseClassRecord = {};
					if (getLeafRecordByIndex(typeRecords, member.baseClass.classTypeIndex, baseClassRecord) && isClassStructOrInterface(baseClassRecord.leaf))
					{
						if (baseClassRecord.leaf.class_.propertyField.bIsForwardRef)
						{
							getLeafRecordByIndex(typeRecords, UDTTable::get().getTypeIndex(baseClassRecord.leaf.class_.uniqueName), baseClassRecord);
						}
						writeFieldList(writer, typeRecords, baseClassRecord.leaf.class_.fieldListTypeIndex, bIsFirstElement, baseClassRecord.leaf.class_.name());
					}
				}
				else if (member.type == ELeafType::LEAF_TYPE_VFUNCTAB)
				{
					const char* pTypeName = "void*";
					const char* pAccessAppend = "none";
					switch (member.dataMember.attribField.access)
					{
					case EMemberAccessType::ACCESS_PRIVATE: pAccessAppend = "private"; break;
					case EMemberAccessType::ACCESS_PROTECTED: pAccessAppend = "protected"; break;
					case EMemberAccessType::ACCESS_PUBLIC: pAccessAppend = "public"; break;
					case EMemberAccessType::ACCESS_NO_PROTECTION: break;
					}

					if (!bIsFirstElement)
					{
						writer.append(",\n");
					}
					writer.append("\t\t\t{ \"name\": \"vftp\", \"offset\": %u, \"size\": %u, \"type\": \"%s\", \"access\":\"%s\", \"baseClass\":\"%s\" }",
						member.dataMember.fieldOffset(),
						8,
						pTypeName,
						pAccessAppend,
						(pBaseClass != nullptr) ? pBaseClass : ""
					);
					bIsFirstElement = false;
				}
			}
		}
	}

	enum class StructureEntryType
	{
		TYPE_CLASS,
		TYPE_STRUCT,
		TYPE_INTERFACE,
		TYPE_UNION
	};

	String asString(StructureEntryType type)
	{
		switch (type)
		{
		case pdb::StructureEntryType::TYPE_CLASS:
			return "class";
		case pdb::StructureEntryType::TYPE_STRUCT:
			return "struct";
		case pdb::StructureEntryType::TYPE_INTERFACE:
			return "interface";
		case pdb::StructureEntryType::TYPE_UNION:
			return "union";
		default:
			return "undefined";
		}
	}

	enum class MemberEntryAccessType
	{
		ACCESS_NONE,
		ACCESS_PROTECTED,
		ACCESS_PUBLIC,
		ACCESS_PRIVATE
	};
	
	String asString(MemberEntryAccessType type)
	{
		switch (type)
		{
		case pdb::MemberEntryAccessType::ACCESS_NONE:
			return "";
		case pdb::MemberEntryAccessType::ACCESS_PROTECTED:
			return "protected";
		case pdb::MemberEntryAccessType::ACCESS_PUBLIC:
			return "public";
		case pdb::MemberEntryAccessType::ACCESS_PRIVATE:
			return "private";
		default:
			return "undefined";
		}
	}

	struct MemberEntry
	{
		bool isPadding = false;
		bool isVirtualBase = false;
		String name;
		String type;
		String baseType;
		uint32_t size = 0;
		uint32_t offset = 0;
		MemberEntryAccessType access = MemberEntryAccessType::ACCESS_NONE;
		String baseClass;
		int64_t typeIndex = -1;
		int64_t baseClassIndex = -1;
	};

	struct StructureEntry
	{
		StructureEntryType type = StructureEntryType::TYPE_CLASS;
		String name;
		String filePath;
		uint32_t lineNumber = 0;
		float wastage = 0.0f;
		int32_t padding = 0;
		bool packed = false;
		uint32_t size = 0;
		TArray<MemberEntry> members;
		uint32_t memberNum = 0;
		uint64_t index = 0;
		uint64_t hash = 0;
		bool hasVirtualBases = false;
		int32_t alignment = -1;
	};

	bool operator==(const StructureEntry& a, const StructureEntry& b)
	{
		return a.hash == b.hash;
	}

	struct StringHasher
	{
		size_t operator()(const String& s) const
		{
			return hash::hash(s);
		}
	};

	struct StringEqual
	{
		bool operator()(const String& a, const String& b) const { return a == b; }
	};

	void outputTypeLayoutReport(const char* pOutputFileName, TArray<TypeRecord>& typeRecords, void* callbackFunc)
	{
		WriteBuffer writer(1024 * 1024 * 200);
		writer.append("[\n");
		bool bIsFirstEntry = true;
		for (uint64_t index = 0; index < typeRecords.getNum(); ++index)
		{
			const TypeRecord& record = typeRecords[index];

			if ((record.leaf.type == ELeafType::LEAF_TYPE_CLASS ||
				record.leaf.type == ELeafType::LEAF_TYPE_STRUCTURE ||
				record.leaf.type == ELeafType::LEAF_TYPE_INTERFACE)
				&& !record.leaf.class_.propertyField.bIsForwardRef)
			{
				if (!bIsFirstEntry)
				{
					writer.append(",\n");
				}
				writer.append("\t{\n");
				LeafPropertyField propertyField = record.leaf.class_.propertyField;
				ELeafType type = record.leaf.type;
				switch (type)
				{
				case ELeafType::LEAF_TYPE_CLASS:
					writer.append("\t\t\"type\":\"class\",\n");
					break;
				case ELeafType::LEAF_TYPE_STRUCTURE:
					writer.append("\t\t\"type\":\"struct\",\n");
					break;
				case ELeafType::LEAF_TYPE_INTERFACE:
					writer.append("\t\t\"type\":\"interface\",\n");
					break;
				default:
					break;
				}

				writer.append("\t\t\"packed\":\"%s\",\n", propertyField.bPacked ? "true" : "false");
				writer.append("\t\t\"name\":\"%s\",\n", record.leaf.class_.name());
				writer.append("\t\t\"size\":%u,\n", record.leaf.class_.sizeOf());
				writer.append("\t\t\"members\":[\n");
				if (record.leaf.class_.fieldListTypeIndex > 0)
				{
					bool bIsFirstElement = true;
					writeFieldList(writer, typeRecords, record.leaf.class_.fieldListTypeIndex, bIsFirstElement);
				}
				writer.append("\n\t\t]\n");
				writer.append("\t}");
				bIsFirstEntry = false;
			}
			else if (record.leaf.type == ELeafType::LEAF_TYPE_UNION && !record.leaf.union_.propertyField.bIsForwardRef)
			{
				if (!bIsFirstEntry)
				{
					writer.append(",\n");
				}
				writer.append("\t{\n");
				LeafPropertyField propertyField = record.leaf.union_.propertyField;

				writer.append("\t\t\"type\":\"union\",\n");
				writer.append("\t\t\"packed\":\"%s\",\n", propertyField.bPacked ? "true" : "false");
				writer.append("\t\t\"name\":\"%s\",\n", record.leaf.union_.name());
				writer.append("\t\t\"size\":%u,\n", record.leaf.union_.sizeOf());
				writer.append("\t\t\"members\":[\n");
				if (record.leaf.union_.fieldListTypeIndex > 0)
				{
					bool bIsFirstElement = true;
					writeFieldList(writer, typeRecords, record.leaf.union_.fieldListTypeIndex, bIsFirstElement);
				}
				writer.append("\n\t\t]\n");
				writer.append("\t}");
				bIsFirstEntry = false;
			}
		}
		writer.append("\n]");
		writer.flush(pOutputFileName, "-TypeLayout", "json", callbackFunc);
	}

	void getFieldList(const TArray<TypeRecord>& typeRecords, TArray<MemberEntry>& output,
		uint32_t fieldListTypeIndex, bool& bIsFirstElement,
		const char* pBaseClass = nullptr,
		uint32_t baseClassOffset = 0,
		TArray<uint32_t>* pEmittedVfptrOffsets = nullptr,
		TArray<uint32_t>* pEmittedVbptrOffsets = nullptr,
		TArray<uint32_t>* pEmittedVirtualBases = nullptr,
		bool isVirtualBaseContext = false)
	{
		TArray<uint32_t> localEmittedVfptrOffsets;
		TArray<uint32_t> localEmittedVbptrOffsets;
		TArray<uint32_t> localEmittedVirtualBases;
		if (pEmittedVfptrOffsets == nullptr) pEmittedVfptrOffsets = &localEmittedVfptrOffsets;
		if (pEmittedVbptrOffsets == nullptr) pEmittedVbptrOffsets = &localEmittedVbptrOffsets;
		if (pEmittedVirtualBases == nullptr) pEmittedVirtualBases = &localEmittedVirtualBases;

		struct PendingVirtualBase
		{
			uint32_t typeIndex;
			uint32_t fieldListTypeIndex;
			String name;
		};
		TArray<PendingVirtualBase> pendingVirtualBases;

		TypeRecord record = {};
		if (getLeafRecordByIndex(typeRecords, fieldListTypeIndex, record) && record.leaf.type == ELeafType::LEAF_TYPE_FIELDLIST)
		{
			uint16_t num = record.leaf.fieldList.fieldNum;
			for (uint16_t index = 0; index < num; ++index)
			{
				const MemberRecord& member = record.leaf.fieldList.members[index];
				if (member.type == ELeafType::LEAF_TYPE_MEMBER)
				{
					MemberEntry entry{};
					TypeInfo info = getTypeInfo(typeRecords, member.dataMember.typeIndexRecordField);
					const char* pTypeName = info.size > 0 ? info.typeName() : "undefined type";
					if (info.pDirectName != nullptr)
						pTypeName = info.pDirectName;

					const char* pPointerAppend = "";
					if (info.bIsPtr)
					{
						if (info.ptrMode == EPointerMode::MODE_LVALUE_REF) pPointerAppend = "&";
						else if (info.ptrMode == EPointerMode::MODE_RVALUE_REF) pPointerAppend = "&&";
						else pPointerAppend = "*";
					}

					entry.access = MemberEntryAccessType::ACCESS_NONE;
					switch (member.dataMember.attribField.access)
					{
					case EMemberAccessType::ACCESS_PRIVATE:       entry.access = MemberEntryAccessType::ACCESS_PRIVATE; break;
					case EMemberAccessType::ACCESS_PROTECTED:     entry.access = MemberEntryAccessType::ACCESS_PROTECTED; break;
					case EMemberAccessType::ACCESS_PUBLIC:        entry.access = MemberEntryAccessType::ACCESS_PUBLIC; break;
					case EMemberAccessType::ACCESS_NO_PROTECTION: entry.access = MemberEntryAccessType::ACCESS_NONE; break;
					}
					entry.isVirtualBase = isVirtualBaseContext;
					entry.name = member.dataMember.name();
					entry.offset = baseClassOffset + member.dataMember.fieldOffset();
					entry.size = info.size;
					entry.type = str::format("%s%s%s", pTypeName, pPointerAppend, (info.arraySize > 0 ? TempFmtByteBuffer("[%u]", info.arraySize) : ""));
					entry.baseType = pTypeName;
					entry.baseClass = (pBaseClass != nullptr) ? pBaseClass : "";

					bIsFirstElement = false;
					if (entry.size > 0)
						output.add(entry);
				}
				else if (member.type == ELeafType::LEAF_TYPE_BCLASS ||
					member.type == ELeafType::LEAF_TYPE_BINTERFACE)
				{
					TypeRecord baseClassRecord = {};
					if (getLeafRecordByIndex(typeRecords, member.baseClass.classTypeIndex, baseClassRecord) && isClassStructOrInterface(baseClassRecord.leaf))
					{
						if (baseClassRecord.leaf.class_.propertyField.bIsForwardRef)
							getLeafRecordByIndex(typeRecords, UDTTable::get().getTypeIndex(baseClassRecord.leaf.class_.uniqueName), baseClassRecord);

						getFieldList(typeRecords, output,
							baseClassRecord.leaf.class_.fieldListTypeIndex,
							bIsFirstElement,
							baseClassRecord.leaf.class_.name(),
							member.baseClass.subObjectOffset(),
							pEmittedVfptrOffsets,
							pEmittedVbptrOffsets,
							pEmittedVirtualBases,
							isVirtualBaseContext);
					}
				}
				else if (member.type == ELeafType::LEAF_TYPE_VFUNCTAB)
				{
					uint32_t vfptrOffset = baseClassOffset;

					bool alreadyEmitted = false;
					for (uint32_t emitted : *pEmittedVfptrOffsets)
						if (emitted == vfptrOffset) { alreadyEmitted = true; break; }
					if (alreadyEmitted) continue;
					pEmittedVfptrOffsets->add(vfptrOffset);

					String vfptrClassName = "";
					TypeRecord pointerRecord = {};
					if (getLeafRecordByIndex(typeRecords, member.vTablePtr.pointerTypeIndex, pointerRecord) &&
						pointerRecord.leaf.type == ELeafType::LEAF_TYPE_POINTER)
					{
						uint32_t underlyingIndex = pointerRecord.leaf.pointer.underlyingType;
						TypeRecord underlyingRecord = {};
						if (getLeafRecordByIndex(typeRecords, underlyingIndex, underlyingRecord))
						{
							if (underlyingRecord.leaf.type == ELeafType::LEAF_TYPE_MODIFIER)
								getLeafRecordByIndex(typeRecords, underlyingRecord.leaf.modifier.typeIndex, underlyingRecord);

							if (underlyingRecord.leaf.type == ELeafType::LEAF_TYPE_VFTABLE)
							{
								TypeRecord classRecord = {};
								if (getLeafRecordByIndex(typeRecords, underlyingRecord.leaf.vfTable.completeClass, classRecord) &&
									isClassStructOrInterface(classRecord.leaf))
								{
									vfptrClassName = classRecord.leaf.class_.name();
								}
							}
						}
					}

					const char* displayName = (pBaseClass != nullptr && pBaseClass[0] != '\0')
						? pBaseClass
						: *vfptrClassName;

					MemberEntry entry{};
					entry.isVirtualBase = isVirtualBaseContext;
					entry.name = (displayName != nullptr && displayName[0] != '\0')
						? str::format("vfptr [%s]", displayName)
						: String("vfptr");
					entry.offset = vfptrOffset;
					entry.size = (uint32_t)pointerSize;
					entry.type = "void*";
					entry.baseClass = (pBaseClass != nullptr) ? pBaseClass : "";
					bIsFirstElement = false;
					output.add(entry);
				}
				else if (member.type == ELeafType::LEAF_TYPE_VBCLASS ||
					member.type == ELeafType::LEAF_TYPE_IVBCLASS)
				{
					uint32_t virtualBaseTypeIndex = member.virtualBaseClass.directVirtualBaseClassTypeIndex;
					uint32_t vbptrOffset = baseClassOffset + member.virtualBaseClass.vbPtrAddressOffset();

					String vbClassName = "";
					TypeRecord resolvedBase = {};
					if (getLeafRecordByIndex(typeRecords, virtualBaseTypeIndex, resolvedBase) &&
						isClassStructOrInterface(resolvedBase.leaf))
					{
						if (resolvedBase.leaf.class_.propertyField.bIsForwardRef)
							getLeafRecordByIndex(typeRecords, UDTTable::get().getTypeIndex(resolvedBase.leaf.class_.uniqueName), resolvedBase);
						vbClassName = resolvedBase.leaf.class_.name();
					}

					bool ptrAlreadyEmitted = false;
					for (uint32_t emitted : *pEmittedVbptrOffsets)
						if (emitted == vbptrOffset) { ptrAlreadyEmitted = true; break; }

					if (!ptrAlreadyEmitted)
					{
						pEmittedVbptrOffsets->add(vbptrOffset);

						MemberEntry vbptrEntry{};
						vbptrEntry.name = vbClassName.length() > 0 ? str::format("vbptr [%s]", *vbClassName) : String("vbptr");
						vbptrEntry.offset = vbptrOffset;
						vbptrEntry.size = (uint32_t)pointerSize;
						vbptrEntry.type = "void*";
						vbptrEntry.baseClass = (pBaseClass != nullptr) ? pBaseClass : "";
						vbptrEntry.isVirtualBase = isVirtualBaseContext;
						bIsFirstElement = false;
						output.add(vbptrEntry);
					}

					bool baseAlreadyRecursed = false;
					for (uint32_t emitted : *pEmittedVirtualBases)
						if (emitted == virtualBaseTypeIndex) { baseAlreadyRecursed = true; break; }

					if (!baseAlreadyRecursed && resolvedBase.index > 0)
					{
						pEmittedVirtualBases->add(virtualBaseTypeIndex);
						pendingVirtualBases.add({ virtualBaseTypeIndex, resolvedBase.leaf.class_.fieldListTypeIndex, vbClassName });
					}
				}
				else if (member.type == ELeafType::LEAF_TYPE_INDEX)
				{
					getFieldList(typeRecords, output, member.leafIndex.leafTypeIndex,
						bIsFirstElement, pBaseClass, baseClassOffset,
						pEmittedVfptrOffsets, pEmittedVbptrOffsets, pEmittedVirtualBases, isVirtualBaseContext);
				}
			}

			for (uint64_t i = 0; i < pendingVirtualBases.getNum(); ++i)
			{
				const PendingVirtualBase& pending = pendingVirtualBases[i];

				TArray<uint32_t> vbaseVfptrOffsets;
				TArray<uint32_t> vbaseVbptrOffsets;

				getFieldList(typeRecords, output,
					pending.fieldListTypeIndex,
					bIsFirstElement,
					*pending.name,
					0,
					&vbaseVfptrOffsets,
					&vbaseVbptrOffsets,
					pEmittedVirtualBases, true);
			}
		}
	}

#undef max
#undef min

	void ouputClassLayoutData(const TArray<TypeRecord>& typeRecords, TArray<StructureEntry>& output)
	{
		std::unordered_map<String, uint64_t, StringHasher, StringEqual> classNameOffset;
		std::unordered_map<uint64_t, bool> uniqueMap;
		LOG("Generating Table");

		for (uint64_t index = 0; index < typeRecords.getNum(); ++index)
		{
			const TypeRecord& record = typeRecords[index];

			if ((record.leaf.type == ELeafType::LEAF_TYPE_CLASS ||
				record.leaf.type == ELeafType::LEAF_TYPE_STRUCTURE ||
				record.leaf.type == ELeafType::LEAF_TYPE_INTERFACE)
				&& !record.leaf.class_.propertyField.bIsForwardRef)
			{
				StructureEntry entry{};

				LeafPropertyField propertyField = record.leaf.class_.propertyField;
				ELeafType type = record.leaf.type;
				switch (type)
				{
				case ELeafType::LEAF_TYPE_CLASS:
					entry.type = StructureEntryType::TYPE_CLASS;
					break;
				case ELeafType::LEAF_TYPE_STRUCTURE:
					entry.type = StructureEntryType::TYPE_STRUCT;
					break;
				case ELeafType::LEAF_TYPE_INTERFACE:
					entry.type = StructureEntryType::TYPE_INTERFACE;
					break;
				default:
					break;
				}
				entry.packed = propertyField.bPacked;
				entry.name = record.leaf.class_.name();
				entry.size = record.leaf.class_.sizeOf();

				if (record.leaf.class_.fieldListTypeIndex > 0)
				{
					bool bIsFirstElement = true;
					getFieldList(typeRecords, entry.members, record.leaf.class_.fieldListTypeIndex, bIsFirstElement);
					std::sort(entry.members.begin(), entry.members.end(), [](const MemberEntry& a, const MemberEntry& b)
					{
						if (a.isVirtualBase != b.isVirtualBase)
							return !a.isVirtualBase;

						if (a.isVirtualBase && b.isVirtualBase)
						{
							bool aIsPtr = a.type == "void*";
							bool bIsPtr = b.type == "void*";
							if (aIsPtr != bIsPtr)
								return aIsPtr;
						}
						return a.offset < b.offset;
					});
				}

				entry.memberNum = (uint32_t)entry.members.getNum();
				if (entry.memberNum > 0)
				{
					classNameOffset.insert(std::pair<String, uint64_t>(entry.name, output.getNum()));
					entry.index = output.getNum();

					UDTSrcLineTable::Entry srcEntry{};
					if (UDTSrcLineTable::get().get(record.index, srcEntry))
					{
						entry.filePath = IPIStringTable::get().getByOffset(srcEntry.srcFileIndex);
						entry.lineNumber = srcEntry.lineNumber;
					}

					entry.hash = hash::hash(entry.name);
					if (uniqueMap.find(entry.hash) == uniqueMap.end())
					{
						output.add(entry);
						uniqueMap.insert({ entry.hash, true });
					}
				}
			}
			else if (record.leaf.type == ELeafType::LEAF_TYPE_UNION && !record.leaf.union_.propertyField.bIsForwardRef)
			{
				LeafPropertyField propertyField = record.leaf.union_.propertyField;
				StructureEntry entry{};
				entry.type = StructureEntryType::TYPE_UNION;
				entry.packed = propertyField.bPacked;
				entry.name = record.leaf.union_.name();
				entry.size = record.leaf.union_.sizeOf();
				if (record.leaf.union_.fieldListTypeIndex > 0)
				{
					bool bIsFirstElement = true;
					getFieldList(typeRecords, entry.members, record.leaf.union_.fieldListTypeIndex, bIsFirstElement);
					std::sort(entry.members.begin(), entry.members.end(), [](const MemberEntry& a, const MemberEntry& b)
					{
						if (a.isVirtualBase != b.isVirtualBase)
							return !a.isVirtualBase;

						if (a.isVirtualBase && b.isVirtualBase)
						{
							bool aIsPtr = a.type == "void*";
							bool bIsPtr = b.type == "void*";
							if (aIsPtr != bIsPtr)
								return aIsPtr;
						}
						return a.offset < b.offset;
					});
				}
				entry.memberNum = (uint32_t)entry.members.getNum();
				if (entry.memberNum > 0)
				{
					classNameOffset.insert(std::pair<String, uint64_t>(entry.name, output.getNum()));
					entry.index = output.getNum();
					UDTSrcLineTable::Entry srcEntry{};
					if (UDTSrcLineTable::get().get(record.index, srcEntry))
					{
						entry.filePath = IPIStringTable::get().get(srcEntry.srcFileIndex);
						entry.lineNumber = srcEntry.lineNumber;
					}
					entry.hash = hash::hash(entry.name);
					if (uniqueMap.find(entry.hash) == uniqueMap.end())
					{
						output.add(entry);
						uniqueMap.insert({ entry.hash, true });
					}
				}
			}
		}

		// Post process
		// Handle Type Indices
		for (StructureEntry& entry : output)
		{
			for (MemberEntry& member : entry.members)
			{
				member.typeIndex = -1;
				member.baseClassIndex = -1;

				if (classNameOffset.find(member.type) != classNameOffset.end())
				{
					member.typeIndex = (int64_t)classNameOffset[member.type];
				}
				else if (classNameOffset.find(member.baseType) != classNameOffset.end())
				{
					member.typeIndex = (int64_t)classNameOffset[member.baseType];
				}

				if (classNameOffset.find(member.baseClass) != classNameOffset.end())
				{
					member.baseClassIndex = (int64_t)classNameOffset[member.baseClass];
				}
			}
		}



		LOG("Calculating Padding");
		// Add Padding
		for (StructureEntry& entry : output)
		{
			TArray<MemberEntry> paddedMembers;
			uint32_t totalPadding = 0;
			uint32_t expectedOffset = 0;
			bool hasVirtualBaseMembers = false;

			for (uint64_t index = 0; index < entry.members.getNum(); ++index)
			{
				const MemberEntry& member = entry.members[index];

				if (member.isVirtualBase)
				{
					hasVirtualBaseMembers = true;
					paddedMembers.add(member);
					continue;
				}

				if (entry.type != StructureEntryType::TYPE_UNION && member.size > 0)
				{
					if (member.offset > expectedOffset)
					{
						uint32_t padding = member.offset - expectedOffset;
						MemberEntry paddingEntry{};
						paddingEntry.isPadding = true;
						paddingEntry.size = padding;
						paddingEntry.offset = expectedOffset;
						paddingEntry.name = "padding";
						paddedMembers.add(paddingEntry);
						totalPadding += padding;
					}
					expectedOffset = math::max(expectedOffset, member.offset + member.size);
				}

				paddedMembers.add(member);
			}

			if (!hasVirtualBaseMembers && entry.type != StructureEntryType::TYPE_UNION && expectedOffset < entry.size)
			{
				uint32_t padding = entry.size - expectedOffset;
				MemberEntry paddingEntry{};
				paddingEntry.isPadding = true;
				paddingEntry.size = padding;
				paddingEntry.offset = expectedOffset;
				paddingEntry.name = "padding";
				paddedMembers.add(paddingEntry);
				totalPadding += padding;
			}

			entry.members = paddedMembers;
			entry.padding = totalPadding;
			entry.wastage = entry.size > 0 ? (float)math::min(totalPadding, entry.size) / (float)entry.size * 100.0f : 0.0f;

			if (hasVirtualBaseMembers)
			{
				entry.hasVirtualBases = true;
				entry.wastage = 0.0f;
				entry.padding = -1;
			}
		}
	}

	bool process(const void* data, size_t dataSize, TArray<StructureEntry>& output, void* errorFunction)
	{
		globalErrorFunction = (ErrorFunctionType)errorFunction;

		UDTTable::get().clear();
		ByteBufferPool::get().clear();
		MemberRecordTable::get().clear();
		IPIStringTable::get().clear(); 
		UDTSrcLineTable::get().clear();

		TArray<TypeRecord> typeRecords;
		EPDBResult result = processPDB((const uint8_t*)data, dataSize, typeRecords);

		if (result != EPDBResult::RESULT_SUCCESS)
		{
			ALERT("Failed to process PDB\n Error Code (0x%04X) %s \n", result, pdbResultToStr(result));
			return false;
		}

		ouputClassLayoutData(typeRecords, output);
		return true;
	}

} // namespace pdb