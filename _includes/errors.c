#include <elm.h>

int just_do_it()
{
        // THROW a fit.
        PANIC("Shan't!");
        return 42;
}

int main()
{
        // CATCH it, log it, and be cool.
        Error *err;
        PanicReturn ret;
        if(err = TRY(ret)) {
                log_error(dbg_log, ret.error);
                destroy_error(ret.error);
                return -1;
        }

        // Do dangerous things.
        int it = just_do_it();
        NO_WORRIES(ret);

        return it;
}
