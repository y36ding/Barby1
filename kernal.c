// kernal.c
#include "rtx.h"
#include "kernal.h"
#include <signal.h>

pcb* pid_to_pcb(int pid)
{
	switch (pid) {

		case 0 : return pcb_list[0];
		case 1 : return pcb_list[1];
		case 2 : return pcb_list[2];
		case 3 : return pcb_list[3];
		default: return NULL;

	}
}

MsgEnv* k_request_msg_env()
{
	// the real code will keep on trying to search free env
	// queue for envelope and get blocked otherwise
	if (MsgEnvQ_size(free_env_queue) == 0)
		return NULL;

	MsgEnv* free_env = (MsgEnv*)MsgEnvQ_dequeue(free_env_queue);
	return free_env;
}

int k_release_message_env(MsgEnv* env)
{
	if (env == NULL)
		return NULL_ARGUMENT;
	MsgEnvQ_enqueue(free_env_queue, env);
	// check processes blocked for allocate envelope later
	return SUCCESS;
}

int k_send_message(int dest_process_id, MsgEnv *msg_envelope)
{

	if (DEBUG==1) {
		ps("In send message");

		fflush(stdout);
		printf("Dest process id is %i\n",dest_process_id);
		fflush(stdout);
	}
	pcb* dest_pcb =  pid_to_pcb(dest_process_id);

	if (!dest_pcb || !msg_envelope) {
		return NULL_ARGUMENT;
	}

	msg_envelope->sender_pid = current_process->pid;
	msg_envelope->dest_pid = dest_process_id;

	if (DEBUG==1) {

		fflush(stdout);
		printf("Dest pid is %i\n",dest_pcb->pid);
		fflush(stdout);
	}

	MsgEnvQ_enqueue(dest_pcb->rcv_msg_queue, msg_envelope);
	if (DEBUG==1){
		printf("message SENT on enqueued on PID %i and its size is %i\n",dest_pcb->pid,MsgEnvQ_size(dest_pcb->rcv_msg_queue));
		pm(msg_envelope);
	}
	k_log_event(msg_envelope, &send_trace_buf);
	return SUCCESS;
}

MsgEnv* k_receive_message()
{
	if (DEBUG==1) {
		fflush(stdout);
		//printf("Current PCB msgQ size is %i for PID %i\n", MsgEnvQ_size(current_process->rcv_msg_queue), current_process->pid );
	}
	if (current_process->is_i_process == TRUE || current_process->state == NEVER_BLK_RCV)
		return NULL;

	MsgEnv* ret = NULL;
	if (MsgEnvQ_size(current_process->rcv_msg_queue) > 0){
		ret = (MsgEnv*)MsgEnvQ_dequeue(current_process->rcv_msg_queue);
		k_log_event(ret, &receive_trace_buf);
	}
	return ret;
}

int k_send_console_chars(MsgEnv *message_envelope)
{
	if (!message_envelope)
		return NULL_ARGUMENT;


	message_envelope->msg_type = DISPLAY_ACK;
	int retVal = k_send_message(CRT_I_PROCESS_ID, message_envelope);
	crt_i_proc(0);
	return retVal;
}

int k_get_console_chars(MsgEnv *message_envelope)
{
	if (!message_envelope)
		return NULL_ARGUMENT;
	message_envelope->msg_type = CONSOLE_INPUT;
	int retVal = k_send_message( KB_I_PROCESS_ID, message_envelope);

	//current_process = pid_to_pcb(KB_I_PROCESS_ID);
	ps("invoking kbd");
	//kbd_i_proc(0);
	if (DEBUG==1) {
		printf("keyboard process returned to get-console-chars\n");
	}

	return retVal;
}

void atomic(bool state)
{
	if (state != TRUE || state!= FALSE)
		return;

	static sigset_t oldmask;
	sigset_t newmask;
	if (state == TRUE)
	{
		current_process->a_count ++; //every time a primitive is called, increment by 1

		//mask the signals, so atomicity is enforced
		// if first time of atomic on
		if (current_process->a_count == 1)
		{
			sigemptyset(&newmask);
			sigaddset(&newmask, SIGALRM); //the alarm signal
			sigaddset(&newmask, SIGINT); // the CNTRL-C
			sigaddset(&newmask, SIGUSR1); // the KB signal
			sigaddset(&newmask, SIGUSR2); // the CRT signal
			sigprocmask(SIG_BLOCK, &newmask, &oldmask);
		}
	}
	else
	{
		current_process->a_count--; //every time a primitive finishes, decrement by 1
		//if all primitives completes, restore old mask, allow signals
		if (current_process->a_count == 0)
		{
			//restore old mask
			sigprocmask(SIG_SETMASK, &oldmask, NULL);
		}
	}
}

int k_pseudo_process_switch(int pid)
{
	pcb* p = (pcb*)pid_to_pcb(pid);
	if (p == NULL)
		return ILLEGAL_ARGUMENT;
	prev_process = current_process;
	current_process = p;
	return SUCCESS;
}

void k_return_from_switch()
{
	pcb* temp = current_process;
	current_process = prev_process;
	prev_process = current_process;
}

int k_request_delay(int delay, int wakeup_code, MsgEnv *msg_env)
{
#if DEBUG
    printf("%s is requesting a delay of %d with wakeup code %d\n", current_process->name,delay, wakeup_code);
#endif
    msg_env->msg_type = wakeup_code;
    msg_env->time_delay = delay;
    return k_send_message(TIMER_I_PROCESS_ID, msg_env);
}


void k_process_switch(ProcessState next_state)
{
	pcb* next_process = (pcb*)proc_q_dequeue(rdy_proc_queue);
	// Note this is not checking for null process. It is just for checking th dequeue
	// was successful
	if (next_process != NULL)
	{
		current_process->state = next_state;
		pcb* old_process = current_process;
		current_process = next_process;
		k_context_switch(old_process->buf, next_process->buf);
	}
}

void k_context_switch(jmp_buf* prev, jmp_buf* next)
{
	int val = setjmp(*prev);
	if (val == 0)
	{
		longjmp(*next, 1);
	}
}

int k_release_processor()
{
	proc_q_enqueue(current_process);
	k_process_switch(READY);
}

int k_request_process_status(MsgEnv *env)
{
	char* status = (char*)env->data;
	int i;
	for (i = 0; i < PROCESS_COUNT; ++i)
	{
		*status = pcb_list[i]->pid;
		status ++;
		*status = pcb_list[i]->state;
		status++;
		*status = pcb_list[i]->priority;
		status++;
	}
	return SUCCESS;
}

int k_terminate()
{
	cleanup();
}

int k_change_priority(int target_priority, int target_pid)
{
	if (target_priority >= 3 || target_priority < 0)
			return ILLEGAL_ARGUMENT;

	pcb* target_pcb = pid_to_pcb(target_pid);
	if (target_pcb->pid == NULL_PROCESS_ID || target_pcb->is_i_process == TRUE)
		return ILLEGAL_ARGUMENT;

	// if on a ready queue, take if off, change priority, and put it back on
    if(target_pcb->state == READY)
    {
        proc_q_dequeue(rdy_proc_queue, target_pcb);
        target_pcb->priority = target_priority;
        proc_q_enqueue(rdy_proc_queue, target_pcb);
    }
    else
    {
    	target_pcb->priority = target_priority;
    }
    return SUCCESS;
}

int get_trace_tail(TraceBuffer* trace_buff)
{
	return (trace_buff->head + trace_buff->count)%TRACE_LOG_SIZE;
}

//env trace helper function for k_send and k_recieve to log events
int k_log_event(TraceBuffer* trace_buf, MsgEnv *env)
{
	if (env == NULL || trace_buf == NULL)
		return NULL_ARGUMENT;

	int tail = get_trace_tail(trace_buf);
	trace_buf->trace_log[tail].dest_pid = env->dest_pid;
	trace_buf->trace_log[tail].sender_pid = env->sender_pid;
	trace_buf->trace_log[tail].msg_type = env->msg_type;
	trace_buf->trace_log[tail].time_stamp = 4; // Should this be a RTX function?
	if (trace_buf->count == TRACE_LOG_SIZE)
		trace_buf->head = (trace_buf->head + 1)%TRACE_LOG_SIZE;
	else
		trace_buf->count++;
	return SUCCESS;
}

int k_get_trace_buffer( MsgEnv *msg_env )
{
	if (msg_env == NULL)
		return NULL;

    int send_tail = get_trace_tail(&send_trace_buf);
    int receive_tail = get_trace_tail(&receive_trace_buf);
    int i;

    // Assign the memory locations which will be written to in the message envelope
    TraceLog* send_log_stack = (TraceLog*) msg_env->data;
    i = send_trace_buf.head;
    while(i != send_tail)
    {
    	TraceLog* log = &send_trace_buf.trace_log[i];
    	send_log_stack->dest_pid = log->dest_pid;
    	send_log_stack->msg_type = log->msg_type;
    	send_log_stack->sender_pid = log->sender_pid;
    	send_log_stack->time_stamp = log->time_stamp;
    	send_log_stack++;
    	i = (i+1)%TRACE_LOG_SIZE;
    }

    TraceLog* receive_log_stack = send_log_stack + send_trace_buf.count;
    i =  receive_trace_buf.head;
    while(i != receive_tail)
    {
    	TraceLog* log = &receive_trace_buf.trace_log[i];
    	receive_log_stack->dest_pid = log->dest_pid;
    	receive_log_stack->msg_type = log->msg_type;
    	receive_log_stack->sender_pid = log->sender_pid;
    	receive_log_stack->time_stamp = log->time_stamp;
    	receive_log_stack++;
    	i = (i+1)%TRACE_LOG_SIZE;
    }
    return SUCCESS;
}
