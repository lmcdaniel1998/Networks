uint8_t* createPDU(uint32_t sequenceNumber, uint8_t flag, char* payload, int dataLen);

void setChecksum(uint8_t* aPDU, int pduLength);

uint32_t getSeqNum(uint8_t* aPDU, int pduLength);

uint16_t getChecksum(uint8_t* aPDU, int pduLength);

int checkCheckSum(uint8_t* aPDU, int pduLength);

uint16_t getFlag(uint8_t* aPDU, int pduLength);

uint8_t* getPayload(uint8_t* aPDU, int pduLength);

int setFileName(uint32_t windowSize, uint32_t bufferSize, char* filename, uint8_t* payload);

void setRR(uint8_t* payload, uint32_t serverSeqNum);

void setSRJ(uint8_t* payload, uint32_t serverSeqNum);

uint32_t getRR(uint8_t* aPDU, int pduLength);

uint32_t getSRJ(uint8_t* aPDU, int pduLength);

uint32_t getWindowSize(uint8_t* aPDU, int pduLength);

uint32_t getBufferSize(uint8_t* aPDU, int pduLength);

void getFilename(uint8_t* aPDU, int pduLength, char* filename);

void outputPDU(uint8_t* aPDU, int pduLength);
