#include<stdio.h>
#include"ioctl.h"
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>

#define error(x) ({ perror(x); \
		exit(EXIT_FAILURE); })


#define SIZE 8
int main()
{
	char choice[SIZE] = {'\0'};
	char allignment[32] = {'\0'};
	int buff = 0;
	int adc_channel = 0, padc = 0;
	int allign = 1, pa = 0;
	int fd = 0;							// File descriptor for device file


	if( (fd = open("/dev/adc8", O_RDWR)) < 0 ) 
			error("Opening device file");

	strncpy(allignment,"Lower bytes",11);
//	printf("file descriptor: %d\n",fd);
	

	while(1)
	{
		/////////////////// get values of adc channel and type of allignment already set in the driver ///////////////////
		if( ioctl(fd,GET_ALLIGNMENT,&allign) ) 
			error("Ioctl userspace");

		if( ioctl(fd,GET_CHANNEL,&adc_channel) ) 
			error("Ioctl userspace");
		
		if( allign == 1)
			strncpy(allignment,"Lower bytes",11);
	
		else 
			strncpy(allignment,"Upper bytes",11);
			
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		printf("------------------------------------------------------\n");
		printf("| 1. Choose ADC channel\n");
		printf("| 2. Enter data allignment input\n");
		printf("| 3. Get data from ADC\n");
		printf("| 0. Quit\n");
		printf("|\n");
		printf("| Current ADC channel set to: \"%d\"\n", adc_channel);
		printf("| Allignment set to: \"%s\"\n", allignment);
		printf("|\n");
		printf("------------------------------------------------------\n");
		printf("> ");
		fflush(stdout);



		fgets(choice,SIZE,stdin);



		/********************************* CHOICE 0 *****************************************/
		if ( *choice == '0' && (strlen(choice) == 2) ) {
			printf("\nExiting program .....\n\n");
			break;
		}

		/*********************************** CHOICE 1 ***************************************/
		if ( *choice == '1' && (strlen(choice) == 2) ) {
			printf(" ---- Enter ADC channel number(0 to 7): ");
			padc = adc_channel;
			fflush(stdout);
			scanf("%d", &adc_channel);
			while ( getchar() != '\n');														// remove '\n' from stdin
			if ( adc_channel >= 0 && adc_channel <= 7 ) {
				if( ioctl(fd,SET_CHANNEL,&adc_channel) ) 
					error("Ioctl userspace");

				printf(" \n\t ADC channel value set successfully to : %d\n\n",adc_channel);
			}
			else {
				printf("\n\tEnter valid ADC channel number\n\n");
				adc_channel = padc;
			}
		}


		/*********************************** CHOICE 2 ****************************************/
		else if( *choice == '2' && (strlen(choice) == 2) ) {
			printf(" ---- Enter allignment choice (\"1\" for lower alligned and \"2\" for upper alligned): ");
			fflush(stdout);
			pa = allign;
			scanf("%d", &allign);
			while ( getchar() != '\n');														// remove '\n' from stdin
			if ( allign == 1 || allign == 2 ) {
				if( ioctl(fd,SET_ALLIGNMENT,&allign) ) 
					error("Ioctl userspace");
					
				printf("\n\t Allignment value set to %d successfully!!!!\n\n",allign);
				if ( allign == 1 )
					strncpy(allignment,"Lower bytes",11);
				else
					strncpy(allignment,"Upper bytes",11);
				}
			else {
				printf("\n\tEnter valid value\n\n");
				allign = pa;
			}
		}


		/********************************** CHOICE 3 ********************************************/
		else if( *choice == '3' && (strlen(choice) == 2) ) {
			int rret;
			printf(" \nGetting data for ADC channel.... \'%d\'\n",adc_channel);
			if ( ( rret = read(fd,&buff,2)) == -1 ) 
				error("read");
			
			printf("\t DATA: %d\n",buff);
		}

		else {
			printf("\n\tEnter valid choice\n\n");
		}
		
	}

	close(fd);
	return 0;
}
