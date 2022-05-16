#include <iostream>
#include "./test/test_strand.h"

int main(int argc,char** argv)
{
  everest::LoggerCreateInfo info;
  info.level = spdlog::level::info;
  info.rotate = false;
  info.to_stdout = true;
  info.async_logger = true;

  CREATE_DEFAULT_LOGGER("test_strand", info);

  {
    TestStrand test_strand(10,1,10);
    LOG_INFO("create test_strand success");
    test_strand.MainThreadRun();
  }

  //system("pause");
  return 0;
}