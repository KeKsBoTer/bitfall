#define _GNU_SOURCE
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sched.h>
#include <pthread.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <inttypes.h>
#include <signal.h>

#define HB_ENERGY_IMPL
#include <heartbeats/hb-energy.h>
#include <heartbeats/heartbeat-accuracy.h>
#include <poet/poet.h>
#include <poet/poet_config.h>

#include "db.h"
#include "bptree.h"

// POET / HEARBEAT related stuff

#define PREFIX "BPTREE"

#define USE_POET // Power and performance control

heartbeat_t *heart;
poet_state *state;
static poet_control_state_t *control_states;
static poet_cpu_state_t *cpu_states;

int heartbeats_counter;

void hb_poet_init()
{
    float min_heartrate;
    float max_heartrate;
    int window_size;
    double power_target;
    unsigned int nstates;

    heartbeats_counter = 0;

    if (getenv(PREFIX "_MIN_HEART_RATE") == NULL)
    {
        min_heartrate = 300.0;
    }
    else
    {
        min_heartrate = atof(getenv(PREFIX "_MIN_HEART_RATE"));
    }
    if (getenv(PREFIX "_MAX_HEART_RATE") == NULL)
    {
        max_heartrate = 400.0;
    }
    else
    {
        max_heartrate = atof(getenv(PREFIX "_MAX_HEART_RATE"));
    }
    if (getenv(PREFIX "_WINDOW_SIZE") == NULL)
    {
        window_size = 100;
    }
    else
    {
        window_size = atoi(getenv(PREFIX "_WINDOW_SIZE"));
    }
    if (getenv(PREFIX "_POWER_TARGET") == NULL)
    {
        power_target = 10;
    }
    else
    {
        power_target = atof(getenv(PREFIX "_POWER_TARGET"));
    }

    if (getenv("HEARTBEAT_ENABLED_DIR") == NULL)
    {
        fprintf(stderr, "ERROR: need to define environment variable HEARTBEAT_ENABLED_DIR (see README)\n");
        exit(1);
    }

    printf("init heartbeat with %f %f %d\n", min_heartrate, max_heartrate, window_size);

    heart = heartbeat_acc_pow_init(window_size, 100, "heartbeat.log",
                                   min_heartrate, max_heartrate,
                                   0, 100,
                                   1, hb_energy_impl_alloc(), power_target, power_target);
    if (heart == NULL)
    {
        fprintf(stderr, "Failed to init heartbeat.\n");
        exit(1);
    }
#ifdef USE_POET
    if (get_control_states("config/control_config", &control_states, &nstates))
    {
        fprintf(stderr, "Failed to load control states.\n");
        exit(1);
    }
    if (get_cpu_states("config/cpu_config", &cpu_states, &nstates))
    {
        fprintf(stderr, "Failed to load cpu states.\n");
        exit(1);
    }
    state = poet_init(heart, nstates, control_states, cpu_states, &apply_cpu_config, &get_current_cpu_state, 1, "poet.log");
    if (state == NULL)
    {
        fprintf(stderr, "Failed to init poet.\n");
        exit(1);
    }
#endif
    printf("heartbeat init'd\n");
}

void hb_poet_finish()
{
#ifdef USE_POET
    poet_destroy(state);
    free(control_states);
    free(cpu_states);
#endif
    heartbeat_finish(heart);
    printf("heartbeat finished\n");
}

/* create a dummy data structure */
bptree_t *db_new()
{
    hb_poet_init();

    bptree_t *bptree = malloc(sizeof(bptree_t));
    bptree_init(bptree);

    return bptree;
}

/* wrapper of set command */
int db_put(bptree_t *bptree, key_t key, value_t val)
{
    if (heartbeats_counter % 10000 == 0)
    {
        heartbeat_acc(heart, heartbeats_counter, 1);
        poet_apply_control(state);
    }
    heartbeats_counter++;
    bptree_insert(bptree, key, val);
    return 1;
}

/* wrapper of get command */
bool db_get(bptree_t *bptree, key_t key, value_t *result)
{
    if (heartbeats_counter % 10000 == 0)
    {
        heartbeat_acc(heart, heartbeats_counter, 1);
        poet_apply_control(state);
    }
    heartbeats_counter++;
    return bptree_get(bptree, key, result);
}

/* wrapper of free command */
int db_free(bptree_t *bptree)
{
    hb_poet_finish();

    bptree_free(bptree);
    free(bptree);

    return 0;
}
