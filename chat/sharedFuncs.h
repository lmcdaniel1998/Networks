/*********************************************************/
/* Shared Functions header in Program 2 Chat
 * Author: Luke McDaniel
 * Institution: California Polytechnic State University
 * Course: CPE 464-01 (Networks)
*/
/*********************************************************/

#define FLAG1  0x01
#define FLAG2  0x02
#define FLAG3  0x03
#define FLAG4  0x04
#define FLAG5  0x05
#define FLAG7  0x07
#define FLAG8  0x08
#define FLAG9  0x09
#define FLAG10 0x0A
#define FLAG11 0x0B
#define FLAG12 0x0C
#define FLAG13 0x0D

#define CHATHDRLEN 3

void setTotLen(uint16_t length, char *packet);
void buildChatHdr(uint16_t length, uint8_t flags,
char* packet);
uint16_t getTotLen(char* packet);
uint8_t getFlags(char* packet);
uint16_t buildHandleHdr(uint16_t startIdx, char* handle,
uint8_t handleLen, char* packet);
uint8_t getHandleLen(uint16_t startIdx, char* packet);
void getHandle(uint16_t startIdx, char* handle, char* packet);
