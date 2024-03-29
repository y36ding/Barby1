#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>

#include "rtx.h"
#include "kbcrt.h"
#include "userAPI.h"
#include "rtx_init.h"
#include "procABC.h"

void test_process()
{
	ps("In Test Process");
	k_release_processor();

}

void processP()
{
	//ps("In process P");
	//MsgEnv* env = (MsgEnv*) request_msg_env();
	//send_message(PROCA_ID,env);

	//MsgEnv* env2 = (MsgEnv*)receive_message();
	//while (env2==NULL) {
	//	env2 = (MsgEnv*)receive_message();
	//}

	//return;


    ps("ProcessP Started");
    pp(current_process);
	k_release_processor();
	ps("Back in process P");
    const tWait = 500000;
	MsgEnv* env;
	ps("Requesting env in Proc P");
	env = request_msg_env();
    ps("Envelopes Allocated");

    while(1) {
        ps("Asking for Characters");

        // Request keyboard input
		get_console_chars (env);

		ps("Back in Process P. Keyboard has taken input");
		// Check if keyboard i proc sent a confirmation message
		env = receive_message();
		while(env==NULL) {
			usleep(tWait);
			env = (MsgEnv*)k_receive_message();
			if (env != NULL && env->msg_type == CONSOLE_INPUT)
			{
#if DEBUG
				printf("Keyboard Input Acknowledged");
#endif
			}
		}


		ps("Back to Process P after processing switching from test");

		// Send the input to CRT
		send_console_chars(env);

		// Check if CRT displayed
		env = receive_message();
		while(env==NULL) {
			usleep(tWait);

			env = receive_message();
			if (env != NULL && env->msg_type == DISPLAY_ACK)
			{
				release_message_env(env);
#if DEBUG
				printf("CRT Display Acknowledged");
#endif
			}
		}
	}
    release_message_env(env);

}

void die(int signal)
{
	cleanup();
	printf( "Ending main process ...\n" );
	exit(0);
}

//**************************************************************************
int main()
{

	if (init_globals() != SUCCESS) {
		printf("Globals failed to initilize!");
		cleanup();
	}

	if (init_all_lists() != SUCCESS) {
		printf("Failed to initialize the process and message envelope list. Exiting the OS.\n");
		cleanup();
	}

	if (init_mmaps() != SUCCESS) {
		    	printf("Failed to init mmaps.\n");
		    	cleanup();
	}


    /*MsgEnv* timer_env = request_msg_env();
    k_request_delay(3,WAKEUP10,timer_env);
    MsgEnv* timer_env2 = request_msg_env();
    k_request_delay(6,WAKEUP10,timer_env2);*/


    //processP();
    //ps("PROC A");

    ps("1");
    pp(current_process);
    k_process_switch(READY);

    ps("6");
    //MsgEnv *env = request_msg_env();
    //send_message(PROCA_ID,env);

    while(1);

	// should never reach here, but in case we do, clean up after ourselves
	cleanup();
} // main
