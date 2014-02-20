#ifndef __HT_TEST_H__
#define __HT_TEST_H__

#define HT_TEST_ASSERT(a, msg) \
        if((a)) \
        { \
			  printf("%s:%d - %s\n", __FILE__, __LINE__, msg); \
			  exit(1); \
		  }

#endif //__HT_TEST_H__
