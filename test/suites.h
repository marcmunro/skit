/**
 * @file   suite.h
 * \code
 *     Author:       Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Define test suites for skit.
 */


// Parameters for database connections
#define PGPORT "5432"
#define PGHOST "/var/run/postgresql"


// Global required for cleanup of forked processes
extern SRunner *global_sr;


// The TestRunner function type
typedef int (TestRunner)(void *);

// Suite definition functions
extern Suite *utils_suite(void);
extern Suite *exceptions_suite(void);
extern Suite *objects_suite(void);
extern Suite *options_suite(void);
extern Suite *params_suite(void);
extern Suite *filepaths_suite(void);
extern Suite *xmlfile_suite(void);
extern Suite *relaxng_suite(void);
extern Suite *tsort_suite(void);
extern Suite *deps_suite(void);

// Utility functions
extern Object *objectFromStr(char *instr);
extern boolean objectStrCheck(Object *obj, char *value);
extern void fail_if_contains(char *bufname, char *buffer, 
			     char *expr, char *errstr);
extern void fail_unless_contains(char *bufname, char *buffer, 
				 char *expr, char *errstr);
extern boolean check_test(char *testname, boolean report_this);
extern Document *getDoc(char *name);


// capture.c
extern int captureOutput(TestRunner *test_fn, void *param,
			 char **p_stdout,  char **p_stderr,
			 int *p_signal);

// redirect.c
extern void redirect_stdout(char *filename);
extern void redirect_stderr(char *filename);
extern char *readfrom_stdout(void);
extern char *readfrom_stderr(void);
extern void end_redirects(void);

// testdata.c
void registerTestSQL(void);


// check_params.c
extern void myfail(char *str);
extern boolean contains(char *src, char *expr);


#define TEST_TIMEOUT 180

#define FREEMEMWITHCHECK					\
    skitFreeMem();						\
    fail_if(exceptionCurHandler(),				\
	    "There is still an exception handler in place!\n"); \
    if (memchunks_in_use() != 0) {				\
	showChunks();						\
	fail("There are still %d memory chunks allocated",	\
	     memchunks_in_use());				\
    }								\
    memShutdown()


#define ADD_TEST(p1,p2)				\
    if (check_test(#p2, TRUE)) {		\
        tcase_set_timeout (p1, TEST_TIMEOUT);	\
	tcase_add_test(p1, p2);			\
    }

#define ADD_TEST_RAISE_SIGNAL(p1,p2,p3)			\
    if (check_test(#p2, TRUE)) {			\
        tcase_set_timeout (p1, TEST_TIMEOUT);	\
	tcase_add_test_raise_signal(p1, p2, p3);	\
    }

