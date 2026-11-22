#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "yellow_card.h"

// 옐로카드 관리
// 하나가 아닌 여러개를 저장 가능하게 하기 위해 해시 테이블 사용
#define MAX_YELLOW_CARDS 64

struct yellow_card_entry{
    pid_t tgid;
    bool in;
    bool out;
    struct hlist_node node;
};

static DEFINE_HASHTABLE(yellow_cards, 6);
static DEFINE_RAW_SPINLOCK(yellow_cards_lock);

static unsigned int yellow_card_count;
void add_yellow_card(pid_t tgid){
    struct yellow_card_entry *entry;
    struct yellow_card_entry *new_entry;
    unsigned long flags;

    if (tgid <= 0)
        return;


    new_entry = kmalloc(sizeof(*new_entry), GFP_NOWAIT);
    if (!new_entry)
        return;
    new_entry->tgid = tgid;
    new_entry->in = false;
    new_entry->out = false;

    raw_spin_lock_irqsave(&yellow_cards_lock, flags);

    hash_for_each_possible(yellow_cards, entry, node, tgid) {
        if (entry->tgid == tgid)
            goto unlock;
    }

    if (yellow_card_count < MAX_YELLOW_CARDS){
        hash_add(yellow_cards, &new_entry->node,tgid);
        yellow_card_count++;
        new_entry = NULL;
    }

unlock:
    raw_spin_unlock_irqrestore(&yellow_cards_lock, flags);
    kfree(new_entry);
}

bool check_yellow_card(pid_t tgid){
    struct yellow_card_entry *entry;
    unsigned long flags;
    bool found = false;

    raw_spin_lock_irqsave(&yellow_cards_lock, flags);

    hash_for_each_possible(yellow_cards,entry, node, tgid){
        if (entry->tgid == tgid){
            found = true;
            break;
        }
    }

    raw_spin_unlock_irqrestore(&yellow_cards_lock, flags);

    return found;
}

void remove_yellow_card(pid_t tgid){
    struct yellow_card_entry *entry;
    struct yellow_card_entry *removed = NULL;
    unsigned long flags;

    raw_spin_lock_irqsave(&yellow_cards_lock, flags);

    hash_for_each_possible(yellow_cards, entry, node, tgid) {
        if (entry->tgid == tgid) {
            hash_del(&entry->node);
            yellow_card_count--;
            removed = entry;
            break;
        }
    }

    raw_spin_unlock_irqrestore(&yellow_cards_lock, flags);
    kfree(removed);
}

bool mark_yellow_card(pid_t tgid, int newfd){
    struct yellow_card_entry *entry;
    unsigned long flags;
    bool marked = false;

    if (newfd > 2)
        return false;

    raw_spin_lock_irqsave(&yellow_cards_lock, flags);

    hash_for_each_possible(yellow_cards, entry, node, tgid){
        if (entry->tgid != tgid)
            continue;
        
            switch (newfd){
                case 0:
                    entry->in = true;
                    break;
                case 1:
                    entry->out = true;
                    break;
            }
            marked = true;
            break;
        }
    raw_spin_unlock_irqrestore(&yellow_cards_lock, flags);
    return marked;
}

void clear_yellow_card(){
    struct yellow_card_entry *entry;
    struct hlist_node *next;
    unsigned int bucket;

    hash_for_each_safe(yellow_cards, bucket, next, entry, node) {
        hash_del(&entry->node);
        kfree(entry);
    }
}
