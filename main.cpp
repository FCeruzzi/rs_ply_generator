#include <iostream>
#include <sstream>

#include "rs_ply_generator.hpp"

int main( int argc, char* argv[] )
{
    try{
        RsPlyGenerator generator(argc, argv);
        generator.run();
    } catch( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }

    return 0;
}
