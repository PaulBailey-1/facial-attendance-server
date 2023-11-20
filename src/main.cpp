
#include <iostream>

#include <fmt/core.h>

#include "DBConnection.h"
#include "EntityState.h"

DBConnection db;

void applyUpdate(LongTermStatePtr lts, ShortTermStatePtr sts) {
    // TODO: should be optimal fusion
    for (int i = 0; i < FACE_VEC_SIZE; i++) {
        lts->facialFeatures[i] = sts->facialFeatures[i];
    }
}

void longTermUpdate() {

    std::vector<EntityStatePtr> shortTermStates;
    db.getShortTermStates(shortTermStates);

    for (EntityStatePtr& es : shortTermStates) {
        ShortTermStatePtr sts = std::static_pointer_cast<ShortTermState>(es);
        if (sts->longTermStateKey != -1) {
            LongTermStatePtr lts = db.getLongTermState(sts->longTermStateKey);
            applyUpdate(lts, sts);
        } else {
            db.createLongTermState(sts);
        }
    }

}

int main() {

    db.connect();

    longTermUpdate();
    db.clearShortTermStates();
    
    return 0;
}