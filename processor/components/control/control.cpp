#include "systemc.h"

/* Component: Control module
 *
 * Control module of the processor. Responsible
 * for activating/deactivating functions of
 * the other components at each cycle.
 *
 * Implements internally a Mealy's finite state machine.
 * **/
SC_MODULE(control) {

	public:
		//-- Signals --//
		//-- In --//
		sc_in_clk clock;
		sc_in<sc_uint<4> > opcode;	// OPCODE
		sc_in<sc_uint<8> > of1;		// Operando fonte 1
		sc_in<sc_uint<8> > of2;		// Operando fonte 2
		sc_in<sc_uint<9> > opd;		// Operando destino
		sc_in<bool> N;
		sc_in<bool> Z;

		//-- Out --//
		// Program counter control
		sc_out<bool> enableCP;
		sc_out<bool> loadCP;
		sc_out<bool> resetCP;
		sc_out<sc_uint<9>> jumpValueCP;

		// Instruction memory control
		sc_out<bool> enableIM;
		sc_out<bool> writeIM;

		// Data memory control;
		sc_out<bool> enableDM;
		sc_out<bool> writeDM;

		// Instruction register control
		sc_out<bool> enableRI;
		sc_out<bool> writeRI;
		sc_out<sc_uint<8>> immediateValue;
		sc_out<sc_uint<9>> immediateRegister;

		// Register base control
		sc_out<bool> enableRB;
		sc_out<bool> writeRB;

		// Register pipeline
		sc_out<bool> enableRPL;
		sc_out<bool> writeRPL;
		sc_out<bool> resetRPL;

		// ULA control
		sc_out<bool> resetZN;

		// Multiplex for write data on register base
		sc_out<sc_uint<2>> seletorMultiRBW;

		// Multiplex for addressing DM
		sc_out<sc_uint<2>> seletorMultiDM;
	
		//-- Constructor --//
		SC_CTOR(control) {
			SC_METHOD(state_machine);
			sensitive << clock.pos();
		}	

        unsigned getCycleCount() const { return cycleCount; }

	private:
		//-- Local variables --//
        void (control::*state)() = &control::state_INITIAL;
		bool restartPipe = false;
        unsigned cycleCount = 0;

		//-- Methods for making pipeline easy --//

		/*
		 * Prepare components so that, in the next cycle,
		 * the instruction will be available.
		 * * */
		void prepareReadInstFromIM();

		/* Prepare components so that the instruction
		 * will be loaded into the instruction register in
		 * the next cycle.
		 * * */
		void prepareLoadRI();

		/* Prepare components so that the decoded instruction
		 * will be loaded into the pipeline register.
		 * **/
		void prepareLoadRPL();

		/* Make the instruction waits when reaches the cycle
		 * before the pipeline register (for pipeline purposes
		 * only).
		 * **/
		void stallPipe();

        void prepareJump();
        void prepareConditionalJump();
        void prepareReadRB();
        void prepareWriteRB();
        void prepareReadDM();
        void prepareWriteDM();

        void handleOpcode(unsigned opcode);

		/* The Mealy's state machine: uses the current
		 * state and informations from outside in order
		 * to keep the processor working properly in
		 * terms of microinstructions.
		 * **/
		void state_machine();

        void state_INITIAL();
        void state_INST_READY();
        void state_IR_HAS_INST();
        void state_RPL_READY();
        void state_READY_TO_EXECUTE();
        void state_READY_TO_COMPUTE();
        void state_READY_TO_STORE();
        void state_READY_TO_JUMP();
        void state_RESULT_READY();
        void state_READY_TO_LOAD();

};

void control::state_machine() {
    ++cycleCount;
    (*this.*state)();
}

void control::state_INITIAL() {
    prepareReadInstFromIM();
    state = &control::state_INST_READY;
}

void control::state_INST_READY() {
    prepareLoadRI();
    state = &control::state_IR_HAS_INST;
}

void control::state_IR_HAS_INST() {
    if (restartPipe) {
        restartPipe = false;
        state = &control::state_INITIAL;
    } else { 
        prepareLoadRPL();
        state = &control::state_RPL_READY;
    }
}

void control::state_RPL_READY() {
    enableRPL.write(0);
    prepareReadInstFromIM(); // Take new instruction (pipeline)
    state = &control::state_READY_TO_EXECUTE;
}

enum opcode {
    HALT = 0,
    J = 10,
    JN = 11,
    JZ = 12,
    LD = 8,
    LRI = 13,
    ST = 9,
};

void control::state_READY_TO_EXECUTE() {
    prepareLoadRI(); // Put new instruction on RI (pipeline)
    handleOpcode(opcode.read());
}

void control::handleOpcode(unsigned opcode) 
{
    if (opcode == opcode::LRI) {
        prepareWriteRB();
        immediateRegister.write(opd.read());
        immediateValue.write(of1.read());
        seletorMultiRBW.write(2);
        state = &control::state_RESULT_READY;
    } else if (opcode == opcode::LD) {
        prepareWriteRB();
        prepareReadDM();
        seletorMultiRBW.write(1);
        seletorMultiDM.write(1);
        state = &control::state_READY_TO_LOAD;
    } else if (opcode == opcode::ST) {
        prepareReadRB();
        seletorMultiDM.write(0);
        state = &control::state_READY_TO_STORE;
    } else if (opcode == opcode::J) {
        prepareJump();
        state = &control::state_READY_TO_JUMP;
    } else if (opcode == opcode::JN) {
        if (N.read() == 1) {
            prepareConditionalJump();
        }
        state = &control::state_READY_TO_JUMP;
    } else if (opcode == opcode::JZ) {
        if (Z.read() == 1) {
            prepareConditionalJump();
        }
        state = &control::state_READY_TO_JUMP;
    } else if (opcode == opcode::HALT) {
        sc_stop();
    } else {
        // ULA operations
        seletorMultiRBW.write(0);
        prepareReadRB();
        state = &control::state_READY_TO_COMPUTE;
    }
}

void control::state_READY_TO_COMPUTE() {
    prepareWriteRB();
    stallPipe();
    state = &control::state_RESULT_READY;
}

void control::state_READY_TO_STORE() {
    prepareWriteDM();
    stallPipe();
    state = &control::state_RESULT_READY;
}

void control::state_READY_TO_JUMP() {
    loadCP.write(0);
    state = &control::state_IR_HAS_INST;
}

void control::state_RESULT_READY() {
    enableRB.write(0);
    enableDM.write(0);
    stallPipe();
    state = &control::state_IR_HAS_INST;
}

void control::state_READY_TO_LOAD() {
    prepareWriteRB();
    state = &control::state_RESULT_READY;
}

void control::prepareReadRB() {
    enableRB.write(1);
    writeRB.write(0);
}

void control::prepareWriteRB() {
    enableRB.write(1);
    writeRB.write(1);
}

void control::prepareReadDM() {
    enableDM.write(1);
    writeDM.write(0);
}

void control::prepareWriteDM() {
    enableDM.write(1);
    writeDM.write(1);
}

void control::prepareReadInstFromIM() {
	enableIM.write(1);	// Enable IM
	writeIM.write(0);	// Read from IM
	enableCP.write(1);	// Increment counter
}

void control::prepareLoadRI() {
	enableIM.write(0);	// Disable IM
	enableRI.write(1);	// Enable RI	
	writeRI.write(1);	// Write IR
	enableCP.write(0);	// Stop incrementing PC
}

void control::prepareLoadRPL() {
	enableRI.write(0);	// Disable Ri
	enableRPL.write(1);	// Enable pipeline register 
	writeRPL.write(1);	// Write pipeline register
}

void control::stallPipe() {
	enableRI.write(0);	
}

void control::prepareJump() {
    enableCP.write(0);
    loadCP.write(1);
    jumpValueCP.write(opd);
    restartPipe = true;
}

void control::prepareConditionalJump() {
    prepareJump();
    resetZN.write(1);
}