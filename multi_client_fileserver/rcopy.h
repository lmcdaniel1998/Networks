#define MAXFILELEN 100

typedef struct inputArgs {
	char fromFile[MAXFILELEN + 1];
	char toFile[MAXFILELEN + 1];
	uint32_t windowSize;
	uint32_t bufferSize;
	double errorPercent;
	char remoteMachine[MAXFILELEN + 1];
	int portNumber;
}inputArgs;
