#ifndef SUBORDINATION_GUILE_INIT_HH
#define SUBORDINATION_GUILE_INIT_HH

namespace sbn {
   namespace guile {
        using main_function_type = void (*)(int,char**);
        void main(int argc, char* argv[], main_function_type main);
   }
}

#endif // vim:filetype=cpp
