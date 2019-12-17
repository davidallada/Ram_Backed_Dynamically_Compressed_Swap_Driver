#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

int davidCalloc = 2800000000;
int davidLock = 2600000000;
int aliCalloc = 1100000000;
int aliLock = 1000000000;

int main(int argc, char* argv[]) {
	
	int size_to_lock = 0;
	if(argc == 2)
	{
		size_to_lock = atoi(argv[1]);
	}
	else
	{
		size_to_lock = 500000000;
	}
	char buff[7];
	FILE *in;
	extern FILE *popen();

	// Call command line thing to get available memory
	if(!(in=popen("cat /proc/meminfo | grep \"MemAvailable:\" | awk \'{print $2}\'", "r"""))) {
		exit(1);	
	}
	
	fgets(buff, sizeof(buff), in);
	pclose(in);

	int in_a_row = 4096;
	int current = 0;
	char *to_device = calloc(size_to_lock, sizeof(char));
	int i = 0;
	while(i < size_to_lock)
	{
		if(current == 0)
		{
			to_device[i] = 0;
		}
		else
		{
			to_device[i] = 1;
		}
		i++;
		if(i % in_a_row == 0)
		{
			if(current == 0)
			{
				current = 1;
			}
			else
			{
				current = 0;
			}
		}
	}
	// Convert from kB to B
	long long mem_available = (long long)(((long long)atoi(buff) + (long long)10000) * (long long)10000);
	printf("Mlocking into memory %lld bytes, %d MB\n",mem_available, mem_available/1000000);
	char * arr = calloc(mem_available, sizeof(char));
	int success = mlock(arr, mem_available);
		
	//printf("This is a test program\n");

	//char * arr = calloc(aliCalloc, sizeof(char));
	//int success = mlock(arr, aliLock);

	//printf("Success is %d (0 if yes, 1 if no)\n", success);
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
	//if (arr != NULL) {
	//	printf("arr is success\n");
	//} else {
	//	printf("failure for arr\n");
	//}
	sleep(10);  
	
	return 0;
}
