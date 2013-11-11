

/**
 * Trickle implementation
 *
 * Copyright (C) 2013  INRIA.
 *
 * This file subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 *
 * @ingroup rpl
 * @{
 * @file    trickle.c
 * @brief   Trickle implementation
 * @author  Eric Engel <eric.engel@fu-berlin.de>
 * @}
 */

#include <string.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "inttypes.h"
#include "trickle.h"
#include "rpl/rpl.h"

//TODO in pointer umwandeln, speicher mit malloc holen
char *timer_over_buf;
char *interval_over_buf;
char *dao_delay_over_buf;

char * tvo_delay_over_buf; // trail
int tvo_delay_over_pid; // trail
bool tvo_auto_send = false; // trail
timex_t tvo_time; // trail
vtimer_t tvo_timer; // trail
uint32_t tvo_resend_seconds; // trail
bool tvo_ack_received; // trail
uint8_t tvo_counter; // trail
struct rpl_tvo_local_t * tvo_resend; //trail

char routing_table_buf[RT_STACKSIZE];
int timer_over_pid;
int interval_over_pid;
int dao_delay_over_pid;
int rt_timer_over_pid;

bool ack_received;
uint8_t dao_counter;

uint8_t k;
uint32_t Imin;
uint8_t Imax;
uint32_t I;
uint32_t t;
uint16_t c;
vtimer_t trickle_t_timer;
vtimer_t trickle_I_timer;
vtimer_t dao_timer;
vtimer_t rt_timer;
timex_t t_time;
timex_t I_time;
timex_t dao_time;
timex_t rt_time;

void reset_trickletimer(void)
{
    I = Imin;
    c = 0;
    /* start timer */
    t = (I / 2) + (rand() % (I - (I / 2) + 1));
    t_time = timex_set(0, t * 1000);
    I_time = timex_set(0, I * 1000);
    timex_normalize(&t_time);
    timex_normalize(&I_time);
    vtimer_remove(&trickle_t_timer);
    vtimer_remove(&trickle_I_timer);
    vtimer_set_wakeup(&trickle_t_timer, t_time, timer_over_pid);
    vtimer_set_wakeup(&trickle_I_timer, I_time, interval_over_pid);

}

void init_trickle(void)
{
    timer_over_buf      =  calloc(TRICKLE_TIMER_STACKSIZE, sizeof(char));

    if (timer_over_buf == NULL) {
        puts("[ERROR] Could not allocate enough memory for timer_over_buf!");
        return;
    }

    interval_over_buf   =  calloc(TRICKLE_INTERVAL_STACKSIZE, sizeof(char));

    if (interval_over_buf == NULL) {
        puts("[ERROR] Could not allocate enough memory for interval_over_buf!");
        return;
    }

 //   dao_delay_over_buf  =  calloc(DAO_DELAY_STACKSIZE, sizeof(char));

 //   if (dao_delay_over_buf == NULL) {
 //       puts("[ERROR] Could not allocate enough memory for interval_over_buf!");
 //       return;
 //   }

    //trail
    tvo_delay_over_buf  =  calloc(TVO_DELAY_STACKSIZE,sizeof(char));
        if(tvo_delay_over_buf == NULL){
            puts("[ERROR] Could not allocate enough memory for interval_over_buf!");
            return;
        }

    /* Create threads */
    ack_received = true;
    timer_over_pid = thread_create(timer_over_buf, TRICKLE_TIMER_STACKSIZE,
                                   PRIORITY_MAIN - 1, CREATE_STACKTEST,
                                   trickle_timer_over, "trickle_timer_over");

    interval_over_pid = thread_create(interval_over_buf, TRICKLE_INTERVAL_STACKSIZE,
                                      PRIORITY_MAIN - 1, CREATE_STACKTEST,
                                      trickle_interval_over, "trickle_interval_over");
  //  dao_delay_over_pid = thread_create(dao_delay_over_buf, DAO_DELAY_STACKSIZE,
  //                                     PRIORITY_MAIN - 1, CREATE_STACKTEST,
  //                                     dao_delay_over, "dao_delay_over");
    rt_timer_over_pid = thread_create(routing_table_buf, RT_STACKSIZE,
                                      PRIORITY_MAIN - 1, CREATE_STACKTEST,
                                      rt_timer_over, "rt_timer_over");
    //trail
    tvo_ack_received = true;
    tvo_delay_over_pid = thread_create(tvo_delay_over_buf, TVO_DELAY_STACKSIZE,
                                                                                      PRIORITY_MAIN-1, CREATE_STACKTEST,
                                                                                      tvo_delay_over, "tvo_delay_over");

}

void start_trickle(uint8_t DIOIntMin, uint8_t DIOIntDoubl,
                   uint8_t DIORedundancyConstant)
{
    c = 0;
    k = DIORedundancyConstant;
    Imin = pow(2, DIOIntMin);
    Imax = DIOIntDoubl;
    /* Eigentlich laut Spezifikation erste Bestimmung von I wie auskommentiert: */
    /* I = Imin + ( rand() % ( (Imin << Imax) - Imin + 1 ) ); */
    I = Imin + (rand() % (4 * Imin)) ;

    t = (I / 2) + (rand() % (I - (I / 2) + 1));
    t_time = timex_set(0, t * 1000);
    timex_normalize(&t_time);
    I_time = timex_set(0, I * 1000);
    timex_normalize(&I_time);
    vtimer_remove(&trickle_t_timer);
    vtimer_remove(&trickle_I_timer);
    vtimer_set_wakeup(&trickle_t_timer, t_time, timer_over_pid);
    vtimer_set_wakeup(&trickle_I_timer, I_time, interval_over_pid);
}

void trickle_increment_counter(void)
{
    /* call this function, when received DIO message */
    c++;
}

void trickle_timer_over(void)
{
    ipv6_addr_t mcast;
    ipv6_addr_set_all_nodes_addr(&mcast);

    while (1) {
        thread_sleep();

        /* Laut RPL Spezifikation soll k=0 wie k= Unendlich behandelt werden, also immer gesendet werden */
        if ((c < k) || (k == 0)) {
            send_DIO(&mcast);
        }
    }
}

void trickle_interval_over(void)
{
    while (1) {
        thread_sleep();
        I = I * 2;
        printf("Setting new TRICKLE interval to %"PRIu32" ms\n", I);

        if (I == 0) {
            puts("[WARNING] Interval was 0");

            if (Imax == 0) {
                puts("[WARNING] Imax == 0");
            }

            I = (Imin << Imax);
        }

        if (I > (Imin << Imax)) {
            I = (Imin << Imax);
        }

        c = 0;
        t = (I / 2) + (rand() % (I - (I / 2) + 1));
        /* start timer */
        t_time = timex_set(0, t * 1000);
        timex_normalize(&t_time);
        I_time = timex_set(0, I * 1000);
        timex_normalize(&I_time);

        if (vtimer_set_wakeup(&trickle_t_timer, t_time, timer_over_pid) != 0) {
            puts("[ERROR] setting Wakeup");
        }

        if (vtimer_set_wakeup(&trickle_I_timer, I_time, interval_over_pid) != 0) {
            puts("[ERROR] setting Wakeup");
        }
    }

}

//trail
void tvo_delay_over(void){

        while(1){

                thread_sleep();

                //if((tvo_ack_received == false) && (tvo_counter < TVO_SEND_RETRIES)){
                if(tvo_counter < TVO_SEND_RETRIES){
                        tvo_counter++;
                        rpl_dodag_t * mydodag = rpl_get_my_dodag();

                        /*
                        struct rpl_tvo_t tvo;
                        //rpl_tvo_init(&tvo);

                        memcpy(&tvo, tvo_resend, sizeof(tvo));

                        //printf("\n(checking trickle) tvo_nonce: %u, tvo_rank: %u, resend_nonce: %u, resend_rank: %u \n\n", tvo.nonce, tvo.rank, tvo_resend->nonce, tvo_resend->rank);

                        printf("*RE*");
                        send_TVO(&(tvo_resend->dst_addr), &tvo, NULL);
*/
                        resend_tvos();
                        tvo_time = timex_set(tvo_resend_seconds, 0);
                        vtimer_remove(&tvo_timer);
                        vtimer_set_wakeup(&tvo_timer, tvo_time, tvo_delay_over_pid);
                }
        //        else if (tvo_ack_received == false){
        //                long_delay_tvo();
        //        }
        }
}

//trail
void received_tvo_ack()
{
        printf("\n SETTING TVO_ACK_RECEIVED TO TRUE\n\n");
    tvo_ack_received = true;
    long_delay_tvo();
}

//trail
void set_tvo_auto_send(){
        if(tvo_auto_send == true){
                printf("(trickle.c) setting tvo_auto_send to false (enable with 'a')\n");
                tvo_auto_send = false;
                long_delay_tvo();
        }else{
                printf("(trickle.c) setting tvo_auto_send to true (disable with 'a')\n");
                tvo_auto_send = true;
                //delay_tvo();
        }
}

//trail
void short_delay_tvo(uint32_t seconds){
	printf("setting new TVO delay to %u seconds\n",seconds);
	tvo_time = timex_set(seconds,0);
    tvo_resend_seconds = seconds;
    tvo_counter = 0;
    tvo_ack_received = false;
    vtimer_remove(&tvo_timer);
    vtimer_set_wakeup(&tvo_timer, tvo_time, tvo_delay_over_pid);
}

//trail
void delay_tvo(struct rpl_tvo_local_t * tvo){
        tvo_resend = tvo; //trail
        tvo_time = timex_set(DEFAULT_WAIT_FOR_TVO_ACK,0);
        tvo_counter = 0;
        tvo_ack_received = false;
        vtimer_remove(&tvo_timer);
        vtimer_set_wakeup(&tvo_timer, tvo_time, tvo_delay_over_pid);
}

//trail
void long_delay_tvo(void){
        tvo_time = timex_set(1000000,0);
        tvo_counter = 0;
        tvo_ack_received = false;
        vtimer_remove(&tvo_timer);
        vtimer_set_wakeup(&tvo_timer, tvo_time, tvo_delay_over_pid);
}

void delay_dao(void)
{
    dao_time = timex_set(DEFAULT_DAO_DELAY, 0);
    dao_counter = 0;
    ack_received = false;
    vtimer_remove(&dao_timer);
    vtimer_set_wakeup(&dao_timer, dao_time, dao_delay_over_pid);
}

/* This function is used for regular update of the routes. The Timer can be overwritten, as the normal delay_dao function gets called */
void long_delay_dao(void)
{
    dao_time = timex_set(REGULAR_DAO_INTERVAL, 0);
    dao_counter = 0;
    ack_received = false;
    vtimer_remove(&dao_timer);
    vtimer_set_wakeup(&dao_timer, dao_time, dao_delay_over_pid);
}

void dao_delay_over(void)
{
    while (1) {
        thread_sleep();

        if ((ack_received == false) && (dao_counter < DAO_SEND_RETRIES)) {
            dao_counter++;
            send_DAO(NULL, 0, true, 0);
            dao_time = timex_set(DEFAULT_WAIT_FOR_DAO_ACK, 0);
            vtimer_set_wakeup(&dao_timer, dao_time, dao_delay_over_pid);
        }
        else if (ack_received == false) {
            long_delay_dao();
        }
    }
}

void dao_ack_received()
{
    ack_received = true;
    long_delay_dao();
}

void rt_timer_over(void)
{
    rpl_routing_entry_t *rt;

    while (1) {
        rpl_dodag_t *my_dodag = rpl_get_my_dodag();

        if (my_dodag != NULL) {
            rt = rpl_get_routing_table();

            for (uint8_t i = 0; i < RPL_MAX_ROUTING_ENTRIES; i++) {
                if (rt[i].used) {
                    if (rt[i].lifetime <= 1) {
                        memset(&rt[i], 0, sizeof(rt[i]));
                    }
                    else {
                        rt[i].lifetime--;
                    }
                }
            }

            /* Parent is NULL for root too */
            if (my_dodag->my_preferred_parent != NULL) {
                if (my_dodag->my_preferred_parent->lifetime <= 1) {
                    puts("parent lifetime timeout");
                    rpl_parent_update(NULL);
                }
                else {
                    my_dodag->my_preferred_parent->lifetime--;
                }
            }
        }

        /* Wake up every second */
        vtimer_usleep(1000000);
    }
}

