#ifndef _UTILS_SLAVE_H_

#include "utils.h"

const char *param_list[] = {"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta", "iota", "kappa"};
const char *names[] = {"Kyle", "Dan", "Andrew", "Brian", "Felix"};
const char *new_names[] = {"Kyla", "Dana", "Andrea", "Brianna", "Felicia"};


#define n_param (sizeof (param_list) / sizeof (const char *))
#define n_names (sizeof (names) / sizeof (const char *))
#define nn_names (sizeof (new_names) / sizeof (const char *))

typedef enum
{
    MASTERLESS        = -1,
    HANDSHAKE         =  0,
    MASTERFULL        =  1,
} connection_state_t;

#define _UTILS_SLAVE_H_
#endif /* _UTILS_SLAVE_H_ */