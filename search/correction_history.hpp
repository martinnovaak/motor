#ifndef MOTOR_CORRECTION_HISTORY_HPP
#define MOTOR_CORRECTION_HISTORY_HPP

struct CorrectionHistoryComponents {
    int pawn = 0;
    int threat = 0;
    int nonpawn = 0;
    int minor = 0;
    int major = 0;
    int continuation1 = 0;
    int continuation2 = 0;
};

#endif // MOTOR_CORRECTION_HISTORY_HPP
