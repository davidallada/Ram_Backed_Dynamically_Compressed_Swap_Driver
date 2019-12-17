#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

int davidCalloc = 2800000000;
int davidLock = 2600000000;
int aliCalloc = 1100000000;
int aliLock = 1000000000;

int main(void) {
	printf("This is a test program\n");

	char * arr = calloc(aliCalloc, sizeof(char));
	int success = mlock(arr, aliLock);

	printf("Success is %d (0 if yes, 1 if no)\n", success);
	//for (int i = 0; i < 1100000000; i += 1) {
	//	arr[i] = arr[i] + 5;
		//printf("%d\n", arr[i]);
	//}
	//char * pagedOut = calloc(900000000, sizeof(char));
	//if (pagedOut == NULL) {
	//	printf("pagedOut failed\n");
	//} else {
	//	printf("pagedOut succeeded\n");
	//}
	if (arr != NULL) {
		printf("arr is success\n");
	} else {
		printf("failure for arr\n");
	}
	sleep(10);  
	
	return 0;
}
