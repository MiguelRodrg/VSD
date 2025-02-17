#include <systemc.h>
#include <vector>
#define TEST_SIZE 30

SC_MODULE(Comparator) {
    sc_in<sc_uint<32>> a, b;
    sc_out<bool> a_greater, a_equal, a_smaller;

    void compare() {
        a_greater.write(a.read() > b.read());
        a_equal.write(a.read() == b.read());
        a_smaller.write(a.read() < b.read());
    }

    SC_CTOR(Comparator) {
        SC_METHOD(compare);
        sensitive << a << b;
    }
};

SC_MODULE(Subtractor) {
    sc_in<sc_uint<32>> a, b;
    sc_out<sc_uint<32>> result;
    void subtract() {
        result.write(a.read() - b.read());
    }
    SC_CTOR(Subtractor) {
        SC_METHOD(subtract);
        sensitive << a << b;
    }
};

SC_MODULE(GCD_Controller) {
    sc_in<bool> clk, reset;
    sc_in<sc_uint<32>> a_in, b_in;
    sc_out<sc_uint<32>> gcd_out;
    sc_out<bool> done;

    sc_signal<sc_uint<32>> x, y;
    sc_signal<bool> x_greater, x_equal, x_smaller;
    sc_signal<sc_uint<32>> x_sub_y, y_sub_x;
    sc_signal<int> state;

    enum States {IDLE, LOAD, COMPARE, SUBTRACT_X, SUBTRACT_Y, DONE, WAIT_NEXT_TEST};

    Comparator *comparator;
    Subtractor *subtractor_x, *subtractor_y;

    void fsm_logic() {
      if (reset.read()) {
          state.write(IDLE);
          gcd_out.write(0);
          done.write(false);
      } else {
          switch (state.read()) {
              case IDLE:
                  state.write(LOAD);
                  break;
              case LOAD:
                  x.write(a_in.read());
                  y.write(b_in.read());
                  gcd_out.write(0);
                  state.write(COMPARE);
                  break;
              case COMPARE:
                  if (x_equal.read()) {
                      gcd_out.write(x.read());
                      done.write(true);
                      state.write(DONE);
                  } else if (x_greater.read()) {
                      state.write(SUBTRACT_X);
                  } else {
                      state.write(SUBTRACT_Y);
                  }
                  break;
              case SUBTRACT_X:
                  x.write(x_sub_y.read());
                  state.write(COMPARE);
                  break;
              case SUBTRACT_Y:
                  y.write(y_sub_x.read());
                  state.write(COMPARE);
                  break;
              case DONE:
                  state.write(WAIT_NEXT_TEST);
                  break;
              case WAIT_NEXT_TEST:
                  done.write(false);
                  state.write(LOAD);
                  break;
          }
      }
  }


    SC_CTOR(GCD_Controller) {
        comparator = new Comparator("Comparator");
        subtractor_x = new Subtractor("Subtractor_X");
        subtractor_y = new Subtractor("Subtractor_Y");

        comparator->a(x); comparator->b(y);
        comparator->a_greater(x_greater);
        comparator->a_equal(x_equal);
        comparator->a_smaller(x_smaller);

        subtractor_x->a(x); subtractor_x->b(y); subtractor_x->result(x_sub_y);
        subtractor_y->a(y); subtractor_y->b(x); subtractor_y->result(y_sub_x);

        SC_METHOD(fsm_logic);
        sensitive << clk.pos();
    }

    ~GCD_Controller() {
        delete comparator;
        delete subtractor_x;
        delete subtractor_y;
    }
};

SC_MODULE(Testbench) {
    sc_signal<sc_uint<32>> a_sig, b_sig, gcd_sig;
    sc_signal<bool> done_sig;
    sc_clock clk_sig;
    sc_signal<bool> reset_sig;

    GCD_Controller* gcd_ctrl;
    std::vector<std::pair<int, int>> test_cases;
    int test_index;

    sc_signal<sc_uint<32>> *x_sig, *y_sig;
    sc_signal<int> *state_sig;

    void load_test_cases() {
        test_cases = {{48, 18}, {20, 30}, {35, 10}, {12, 15}, {24, 36}, {40, 8}, {56, 14}, {81, 27}, {90, 10}, {100, 25}, {121, 11}, {64, 16}, {49, 7}, {72, 12}, {99, 33}, {50, 5}, {88, 22}, {96, 48}, {110, 55}, {25, 5}, {77, 7}, {66, 11}, {144, 12}, {33, 3}, {45, 15}, {63, 9}, {85, 5}, {91, 13}, {120, 24}, {150, 30}};
        test_index = 0;
    }

    void run_tests() {
      reset_sig.write(true);
      wait(2, SC_NS);
      reset_sig.write(false);

      for (test_index = 0; test_index < TEST_SIZE; ++test_index) {
          a_sig.write(test_cases[test_index].first);
          b_sig.write(test_cases[test_index].second);

          do {
              wait(1, SC_NS);
          } while (!done_sig.read());

          std::cout << "GCD(" << test_cases[test_index].first << ", "<< test_cases[test_index].second << ") = "<< gcd_sig.read() << std::endl;
          wait(1, SC_NS);
      }
      sc_stop();
  }

    SC_CTOR(Testbench) : clk_sig("clk_sig", 1, SC_NS) {
        gcd_ctrl = new GCD_Controller("GCD_Controller");
        gcd_ctrl->a_in(a_sig);
        gcd_ctrl->b_in(b_sig);
        gcd_ctrl->gcd_out(gcd_sig);
        gcd_ctrl->clk(clk_sig);
        gcd_ctrl->reset(reset_sig);
        gcd_ctrl->done(done_sig);

        x_sig = &(gcd_ctrl->x);
        y_sig = &(gcd_ctrl->y);
        state_sig = &(gcd_ctrl->state);

        load_test_cases();
        SC_THREAD(run_tests);
    }

    ~Testbench() {
        delete gcd_ctrl;
    }
};

int sc_main(int argc, char* argv[]) {
    Testbench tb("Testbench");

    sc_trace_file *wf = sc_create_vcd_trace_file("fsm_waveform");
    wf->set_time_unit(1, SC_PS);

    sc_trace(wf, tb.clk_sig, "clk");
    sc_trace(wf, tb.reset_sig, "reset");
    sc_trace(wf, tb.a_sig, "A");
    sc_trace(wf, tb.b_sig, "B");
    sc_trace(wf, tb.gcd_sig, "GCD");
    sc_trace(wf, *(tb.x_sig), "X");
    sc_trace(wf, *(tb.y_sig), "Y");
    sc_trace(wf, *(tb.state_sig), "FSM_State");
    sc_trace(wf, tb.done_sig, "Done");

    sc_start();
    sc_close_vcd_trace_file(wf);
    return 0;
}
