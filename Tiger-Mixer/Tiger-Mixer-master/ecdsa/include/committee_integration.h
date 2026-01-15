#ifndef COMMITTEE_INTEGRATION_H
#define COMMITTEE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

// 委员会管理常量
#define COMMITTEE_SIZE 3
#define MAX_ADDRESS_LENGTH 42

// 委员会成员结构
typedef struct {
    char address[MAX_ADDRESS_LENGTH];
    bool is_active;
} committee_member_t;

// 委员会状态结构
typedef struct {
    committee_member_t members[COMMITTEE_SIZE];
    uint64_t last_update_time;
    bool system_initialized;
} committee_state_t;

// 函数声明
int committee_integration_init(const char* contract_address, const char* rpc_url);
int get_committee_state(committee_state_t* state);
bool is_committee_member(const char* address);
int get_my_committee_position(const char* my_address);
void committee_integration_cleanup();

// 委员会交互API
int update_reputation(const char* address, int accuracy, int participation, int consistency);
int stake_tokens(const char* address, const char* amount);
int unstake_tokens(const char* address, const char* amount);
int trigger_committee_rotation();
int get_reputation(const char* address);
int get_stake_amount(const char* address);

#endif // COMMITTEE_INTEGRATION_H
