#include <windows.h>
#include <stdint.h>
#include <stdio.h>


unsigned int my_ChkSum(unsigned int checksum, void *BaseAddress, unsigned int length)
{
	uint64_t  acc=checksum;
	uint8_t  *p8=BaseAddress;
	uint32_t *p32;

	for (;length&3;length--) acc+=*p8++;
	p32=(uint32_t *)p8;
	for (;length;length-=4)  acc+=*p32++;

	// Add in the accumulated carry bits and fold the results into a 16-bit value
	acc=(acc>>32)+(acc&0xFFFFFFFF);
	acc+=(acc>>32);
	acc=(acc>>16)+(acc&0xFFFF);
	acc+=(acc>>16);
	acc&=0xFFFF;

	return (uint32_t)acc;
}

void my_CheckSumMappedFile(void *BaseAddress, unsigned int FileLength, unsigned int *CheckSum)
{
	PIMAGE_NT_HEADERS ntHdr=(PIMAGE_NT_HEADERS)(
	                        ((PIMAGE_DOS_HEADER)BaseAddress)->e_lfanew +
							(unsigned char *)BaseAddress);

	unsigned int checksum=0;

	if (BaseAddress && FileLength)
	{
		if (IMAGE_NT_OPTIONAL_HDR32_MAGIC==ntHdr->OptionalHeader.Magic ||
			IMAGE_NT_OPTIONAL_HDR64_MAGIC==ntHdr->OptionalHeader.Magic)
		{
			unsigned int hdrLength=(unsigned char *)ntHdr-(unsigned char *)BaseAddress+0x58;
			
			// Calculate everything before the CheckSum field
			checksum=my_ChkSum(checksum, BaseAddress, hdrLength);
			// Calculate everything after the CheckSum field
			checksum=my_ChkSum(checksum, (void *)((unsigned char *)ntHdr+0x5C), FileLength-hdrLength-0x4);
			// Add the file length to complete the calculation
			checksum+=FileLength;

			*CheckSum=checksum;
		}
	}
}

void * fopen_mem(char *fpath, unsigned int *fsize)
{
	FILE *f;
	void *m;
	
	f = fopen(fpath, "rb");
	fseek(f, 0, SEEK_END);
	*fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	m = malloc(*fsize);
	fread(m, 1, *fsize, f);
	fclose(f);
	
	return m;
}

int main(int argc, char *argv[])
{
	printf("\nPE32 and PE32+ CheckSum Calculator    by    YarnSalesman\n\n");

	if (argc==2)
	{
		unsigned int fsize;
		unsigned int csum;
		void *base=fopen_mem(argv[1], &fsize);
		my_CheckSumMappedFile(base, fsize, &csum);
		free(base);
		printf("    :+:  PE Checksum = 0x%08X\n", csum);
	}
	else
	{
		printf("    :-:  Usage: PECHECKSUM.exe <infile.exe>\n");
	}

	return 0;
}