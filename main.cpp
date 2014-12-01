#include <cstdio>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <regex>

#include "dssim.h"
#include "net.h"
#include "make_unique.h"
#include "args.h"
#include "massert.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>


dssim_t *global_dssim;

void sigint_handler(int s){
  printf("Caught signal %d\n",s);
  global_dssim->pr_stat(l::og(l::INFO));
  printf("Caught signal %d\n",s);
  exit(1); 
}


int main(int argc, char* argv[]) {
   struct sigaction sigIntHandler;

   sigIntHandler.sa_handler = sigint_handler;
   sigemptyset(&sigIntHandler.sa_mask);
   sigIntHandler.sa_flags = 0;

   sigaction(SIGINT, &sigIntHandler, NULL);
   try {
      dssim_t dssim;
      global_dssim = &dssim;
      Net net(&dssim);
      dssim_t::Config con;
      do_args(argc, argv, con);
      for(int i = 0; i < argc; ++i) {
         LOG(l::SHORT, argv[i] << " ");
      }
      LOG(l::SHORT, "\n");
      l::og(l::SHORT) << con;
      dssim.configure(&net, con);
      // Simulation stays alive while there are pending messages
      while(dssim.tick() || net.any_pending())
         ; // Maybe print something periodically.

      LOG(l::SHORT, net << "\n");
      // OMG this is so ugly.  Surely there is a better way.  The problem is that the
      // info to print is nestled deep in these functions and I don't want to pass around
      // new data structures
      if(l::log_level <= l::WARN) {
         dssim.pr_short_short_stat(l::og(l::WARN));
      } else if(l::log_level <= l::SHORT) {
         dssim.pr_short_stat(l::og(l::SHORT));
      } else {
         dssim.pr_stat(l::og(l::INFO));
      }

// Pretty boring
#if 0
      struct rusage ru;
      getrusage(RUSAGE_SELF, &ru);
      l::og(l::SHORT, "RSS %3.2f MB %3.2f GB\n", 
            100.0 * ru.ru_maxrss/(1024*1024), 100.0 * ru.ru_maxrss/(1024*1024*1024));
#endif
   } catch (const std::regex_error& e) {
      std::cerr << "regex_error caught: " << e.what() << '\n';
   }
   return 0;
}
