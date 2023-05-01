#include "../system/system.h"
#include "../system/isecure.h"

// 如果不想验证就注释下面两行

//#define VALIDATE_MAC				// 不想验证mac就注释
// #define VALIDATE_DATE	20180926	// 不想验证日期就注释 (到期后无法运行)
#define VALIDATE_DATE	20210926	// 不想验证日期就注释 (到期后无法运行)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef VALIDATE_MAC


// 合法的mac地址，编辑生效
// 地址一定要写成 windows格式
static const char *valid_mac[] = {
	"00-17-42-EA-73-8D",
	"E0-CB-4E-46-16-24",
	"6C-F0-49-9E-96-C4",
	"00-1D-72-84-59-BA",
	"00-09-6B-A3-E5-0B",
	"1C-6F-65-89-C9-E1",
	"1C-6F-65-AF-9B-3F",
	"EC-55-F9-A5-5A-06",
	"6C-F0-49-9E-96-18",
	NULL,
};



#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif
#include <windows.h>
#include <iphlpapi.h>
#else

#ifndef __unix
#define __unix
#endif

#ifdef __MACH__
#ifndef __FreeBSD__
#define __FreeBSD__
#endif
#endif

#endif


struct CMacAddress
{
	char name[280];
	char mac[64];
};

#ifndef __unix

#include <iphlpapi.h>

int GetMacAddress(struct CMacAddress *address, int *maxsize)
{
	typedef DWORD (WINAPI *GetAdaptersInfo_t)(PIP_ADAPTER_INFO, PULONG);
	static GetAdaptersInfo_t GetAdaptersInfo_o = NULL;
	static HINSTANCE hDLL = NULL;
	PIP_ADAPTER_INFO pAdapterInfo;
	PIP_ADAPTER_INFO pAdapter;
	ULONG count;
	DWORD ret;

	if (GetAdaptersInfo_o == NULL) {
		hDLL = LoadLibraryA("IpHlpAPI.dll");
		if (hDLL == NULL) return -1;
		GetAdaptersInfo_o = (GetAdaptersInfo_t)GetProcAddress(hDLL,
			"GetAdaptersInfo");
		if (GetAdaptersInfo_o == NULL) return -2;
	}

	count = sizeof(IP_ADAPTER_INFO);
	pAdapterInfo = (PIP_ADAPTER_INFO)malloc(count);

	if (pAdapterInfo == NULL)
		return -4;

	ret = GetAdaptersInfo_o(pAdapterInfo, &count);

	if (ret == ERROR_BUFFER_OVERFLOW) {
		free(pAdapterInfo);
		pAdapterInfo = (PIP_ADAPTER_INFO)malloc(count);
		if (pAdapterInfo == NULL)
			return -5;
	}

	ret = GetAdaptersInfo_o(pAdapterInfo, &count);

	if (ret != NO_ERROR) {
		free(pAdapterInfo);
		return -6;
	}

	count = 0;

	for (pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next)
		count++;

	if ((int)count > maxsize[0]) {
		free(pAdapterInfo);
		return count;
	}

	count = 0;

	for (pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
		static const char hex[17] = "0123456789ABCDEF";
		if ((int)count < maxsize[0]) {
			char *ptr = &(address[count].mac[0]);
			int i;
			for (i = 0; i < (int)pAdapter->AddressLength; i++) {
				BYTE ch = pAdapter->Address[i];
				*ptr++ = hex[ch >> 4];
				*ptr++ = hex[ch & 15];
				if (i < (int)pAdapter->AddressLength - 1)
					*ptr++ = '-';
				else 
					*ptr++ = '\0';
			}
			i = strlen(pAdapter->AdapterName) + 1;
			if (i > 260) i = 260;
			memcpy(address[count].name, pAdapter->AdapterName, i);
		}
		count++;
	}

	free(pAdapterInfo);

	maxsize[0] = (int)count;

	return 0;
}

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>

// linux
#if defined(AF_PACKET)		// linux
int GetMacAddress(struct CMacAddress *address, int *maxsize)
{
	struct ifaddrs *int_addrs = NULL;
	const struct ifaddrs *int_cursor = NULL;
	const struct sockaddr_in *dl_addr;
	int count, fd;

	if (getifaddrs(&int_addrs) != 0) {
		return -1;
	}

	int_cursor = int_addrs;
	
	count = 0;
	for (int_cursor = int_addrs; int_cursor != NULL; ) {
		dl_addr = (const struct sockaddr_in*)int_cursor->ifa_addr;
		if (dl_addr != NULL && dl_addr->sin_family == AF_PACKET) {
			count++;
		}
		int_cursor = int_cursor->ifa_next;
	}
	
	if (count > maxsize[0]) {
		freeifaddrs(int_addrs);
		return count;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		freeifaddrs(int_addrs);
		return -2;
	}
	
	count = 0;
	for (int_cursor = int_addrs; int_cursor != NULL; ) {
		dl_addr = (const struct sockaddr_in*)int_cursor->ifa_addr;
		if (dl_addr != NULL && dl_addr->sin_family == AF_PACKET) {
			static const char hex[17] = "0123456789ABCDEF";
			struct ifreq ifr;
			unsigned char *src = ifr.ifr_hwaddr.sa_data;
			char *ptr = address[count].mac;
			int i;

			strncpy(ifr.ifr_name, int_cursor->ifa_name, IFNAMSIZ);
			strncpy(address[count].name, ifr.ifr_name, IFNAMSIZ);
			if (ioctl(fd, SIOCGIFHWADDR, &ifr) != 0) {
				freeifaddrs(int_addrs);
				close(fd);
				return -3;
			}
			for (i = 0; i < 6; i++) {
				unsigned char ch = src[i];
				*ptr++ = hex[ch >> 4];
				*ptr++ = hex[ch & 15];
				if (i < 5) *ptr++ = '-';
				else *ptr++ = '\0';
			}

			count++;
		}
		int_cursor = int_cursor->ifa_next;
	}
	
	freeifaddrs(int_addrs);
	close(fd);

	maxsize[0] = count;

	return 0;
}

#elif defined(SIOCGIFCONF) && defined(SIOCGIFHWADDR)	// cygwin & linux
//SIOCGIFHWADDR, SIOCGIFADDR
int GetMacAddress(struct CMacAddress *address, int *maxsize)
{
	struct ifreq *ifr;
	struct ifreq ifr2;
	struct ifconf ifc;
	int fd, k, i;
	int count;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
		return -1;

	ifc.ifc_len = sizeof(struct ifreq) * 64;
	ifc.ifc_buf = malloc(ifc.ifc_len);

	if (ifc.ifc_buf == NULL) {
		close(fd);
		return -2;
	}

	if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
		free(ifc.ifc_buf);
		ifc.ifc_len = sizeof(struct ifreq) * 1024;
		ifc.ifc_buf = malloc(ifc.ifc_len);
		if (ifc.ifc_buf == NULL) {
			close(fd);
			return -3;
		}
		if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
			free(ifc.ifc_buf);
			close(fd);
			return -4;
		}
	}

	count = 0;
	ifr = ifc.ifc_req;
	for (i = 0; i < ifc.ifc_len; i += sizeof(struct ifreq), ++ifr) {
		ifr2 = *ifr;
		if (ioctl(fd, SIOCGIFHWADDR, &ifr2) == 0) {
			count++;
		}
	}

	if (count > maxsize[0]) {
		free(ifc.ifc_buf);
		close(fd);
		return count;
	}

	count = 0;
	ifr = ifc.ifc_req;
	for (k = 0; k < ifc.ifc_len; k += sizeof(struct ifreq), ++ifr) {
		ifr2 = *ifr;
		if (ioctl(fd, SIOCGIFHWADDR, &ifr2) == 0) {
			static const char hex[17] = "0123456789ABCDEF";
			unsigned char *src = ifr2.ifr_hwaddr.sa_data;
			char *ptr = address[count].mac;
			for (i = 0; i < 6; i++) {
				unsigned char ch = src[i];
				*ptr++ = hex[ch >> 4];
				*ptr++ = hex[ch & 15];
				if (i < 5) *ptr++ = '-';
				else *ptr++ = '\0';
			}
			i = strlen(ifr2.ifr_name) + 1;
			if (i > 132) i = 132;
			memcpy(address[count].name, ifr2.ifr_name, i);
			count++;
		}
	}

	maxsize[0] = count;
	free(ifc.ifc_buf);
	close(fd);

	return 0;
}

#else
#include <net/if_dl.h>

// Maybe freebsd
int GetMacAddress(struct CMacAddress *address, int *maxsize)
{
	struct ifaddrs *ifaphead;
	unsigned char if_mac[512];
	struct ifaddrs *ifap;
	int count = 0;

	if (getifaddrs(&ifaphead) != 0) {
		return -1;
	}
	
	count = 0;
	for (ifap = ifaphead; ifap; ifap = ifap->ifa_next) {
		 if ((ifap->ifa_addr->sa_family == AF_LINK)) {
			 count++;
		 }
	}

	if (count > maxsize[0]) {
		freeifaddrs(ifaphead);
		return count;
	}

	count = 0;
	for (ifap = ifaphead; ifap; ifap = ifap->ifa_next) {
		if ((ifap->ifa_addr->sa_family == AF_LINK)) {
			static const char hex[17] = "0123456789ABCDEF";
			struct sockaddr_dl *sdl = NULL;
			char *ptr = address[count].mac;
			int size, i;
			sdl = (struct sockaddr_dl*)ifap->ifa_addr;
			if (sdl) {
				size = sdl->sdl_alen;
				if (size > 512) size = 512;
				memcpy(if_mac, LLADDR(sdl), size);
				for (i = 0; i < 6; i++) {
					unsigned char ch = if_mac[i];
					*ptr++ = hex[ch >> 4];
					*ptr++ = hex[ch & 15];
					if (i < 5) *ptr++ = '-';
					else *ptr++ = '\0';
				}
			}
			size = strlen(ifap->ifa_name) + 1;
			if (size > 280) size = 280;
			memcpy(address[count].name, ifap->ifa_name, size);
			count++;
		}
	}

	freeifaddrs(ifaphead);
	maxsize[0] = count;

	return 0;
}



#endif


#endif


int validate_mac(void)
{
	CMacAddress *address = (CMacAddress*)malloc(sizeof(CMacAddress) * 1000);
	int maxsize = 1000;
	int i, j;
	if (address == NULL) 
		return -1;
	if (GetMacAddress(address, &maxsize) != 0) {
		free(address);
		return -2;
	}
	for (i = 0; i < maxsize; i++) {
		for (j = 0; valid_mac[j]; j++) {
			if (strcmp(address[i].mac, valid_mac[j]) == 0) {
				free(address);
				return 0;
			}
		}
	}
	free(address);
	return -3;
}


#endif

#ifndef EXPORT_MODULE
	//#ifdef __unix
    #if defined(__unix) || defined(__MACH__)
		#define EXPORT_MODULE(type)  type 
	#elif defined(_WIN32)
		#define EXPORT_MODULE(type)  __declspec(dllexport) type
	#else
		#error APR-EXPORT_MODULE only can be compiled under unix or win32 !!
	#endif
#endif

#ifdef VALIDATE_DATE
#include <time.h>


int validate_date()
{
	time_t tt_now = 0;
	struct tm tm_time, *tmx = &tm_time;
	int timeint;

	tt_now = time(NULL);
	memcpy(&tm_time, localtime(&tt_now), sizeof(tm_time));
	timeint = (tmx->tm_year + 1900) * 10000;
	timeint += (tmx->tm_mon + 1) * 100;
	timeint += tmx->tm_mday;

	if (timeint > VALIDATE_DATE) return -1;

	return 0;
}
#endif

int validate_imp()
{
#ifdef VALIDATE_MAC
	if (validate_mac())
		return -1;
#endif
#ifdef VALIDATE_DATE
	if (validate_date())
		return -2;
#endif
	return 0;
}



//---------------------------------------------------------------------
// 加密解密
//---------------------------------------------------------------------
#if 0
static void QuickKeyMake(std::string &key, const std::string &text)
{
	const char *sign = "Fatal error occured: Out of memory, can not alloc new object !!!";
	int length = (int)text.size();
	int needed = length + 8;
	int random = rand() & 0xff;
	std::string buffer;
	buffer.resize(needed);

	char *buf = &buffer[0];

	buf = iencode16u_lsb(buf, length);
	buf = iencode8u(buf, random);
	buf = iencode8u(buf, 0x4c);
	buf = iencode32u_lsb(buf, hash_crc32(text.c_str(), length));

	CRYPTO_RC4_Crypto(sign, (int)strlen(sign), &text[0], buf, length, 7);
	unsigned char *ptr = (unsigned char*)(&buffer[0]);

	for (int i = 0; i < length + 5; i++) {
		ptr[i + 3] ^= (unsigned char)random;
	}

	for (int k = 0; k < needed; k++) {
		ptr[k] ^= (k & 1)? 0x57 : 0x4c;
	}

	key.resize(ibase16_encode(NULL, needed, NULL) + 10);
	int hr = ibase16_encode(&buffer[0], needed, &key[0]);
	key.resize(hr);
}
#endif

static bool QuickKeyRead(const std::string &key, std::string &text)
{
	const char *sign = "Fatal error occured: Out of memory, can not alloc new object !!!";
	int needed = ((int)key.size()) / 2;
	std::string buffer;

	text.resize(0);
	if (needed == 0) {
		return false;
	}
	
	buffer.resize(needed);

	needed = ibase16_decode(&key[0], (int)key.size(), &buffer[0]);
	buffer.resize(needed);
	int length = needed - 8;

	const char *buf = &buffer[0];

	unsigned char *ptr = (unsigned char*)(&buffer[0]);

	for (int k = 0; k < needed; k++) {
		ptr[k] ^= (k & 1)? 0x57 : 0x4c;
	}

	IUINT16 size;
	IUINT8 random;
	IUINT8 check;
	IUINT32 crc;
	buf = idecode16u_lsb(buf, &size);
	buf = idecode8u(buf, &random);

	if (size != length) return false;

	for (int i = 0; i < length + 5; i++) {
		ptr[i + 3] ^= random;
	}

	buf = idecode8u(buf, &check);
	buf = idecode32u_lsb(buf, &crc);

	if (check != 0x4c) return false;

	text.resize(length);

	CRYPTO_RC4_Crypto(sign, (int)strlen(sign), buf, &text[0], length, 7);

	if (hash_crc32(&text[0], length) != crc) {
		text.resize(0);
		return false;	
	}

	return true;
}

//---------------------------------------------------------------------
// 验证时间
//---------------------------------------------------------------------
#define VD_MAGIC_1 7
#define VD_MAGIC_2 123
#define VD_MAGIC_3 13

static inline bool ValidateDate2(int year, int month, int day)
{
	System::DateTime datetime;
	datetime.localtime();
	int xyear = datetime.year() * VD_MAGIC_1;
	int xmonth = datetime.month() * VD_MAGIC_2;
	int xday = datetime.mday() * VD_MAGIC_3;
	if (xyear > year) return false;
	if (xmonth > month && xyear == year) return false;
	if (xday > day && xyear == year && xmonth == month) return false;
	return true;
}

#define ValidateDate(year, month, day) \
	ValidateDate2((year) * VD_MAGIC_1, (month) * VD_MAGIC_2, (day) * VD_MAGIC_3)

static inline bool QuickValidateKey(const char *activecode)
{
	std::string text;
	int year = -1, month = -1, day = -1;
	if (QuickKeyRead(activecode, text) == false) return false;
	sscanf(text.c_str(), "TH:%04d/%02d/%02d", &year, &month, &day);
	if (year < 0 || month < 0 || day < 0) return false;
	if (ValidateDate(year, month, day) == false) return false;
	return true;
}

static int QuickNet_Permission = -1;

extern "C" int QuickModuleActive(const char *text)
{
	if (QuickValidateKey(text) == true) {
		QuickNet_Permission = 1;
	}	else {
		QuickNet_Permission = 0;
	}
	return QuickNet_Permission;
}


extern "C" int QuickModuleInitialize(void)
{
	static int inited = 0;
	static int result = 0;
	if (inited == 0) {
		result = validate_imp();
		inited = 1;
	}
	if (QuickNet_Permission == 1) return 0;
	return result;
}


