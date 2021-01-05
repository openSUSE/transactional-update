#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <systemd/sd-bus.h>

static int method_multiply(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        int64_t x, y;
        int r;

        /* Read the parameters */
        r = sd_bus_message_read(m, "xx", &x, &y);
        if (r < 0) {
                fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
                return r;
        }

        /* Reply with the response */
        return sd_bus_reply_method_return(m, "x", x * y);
}

static int method_divide(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        int64_t x, y;
        int r;

        /* Read the parameters */
        r = sd_bus_message_read(m, "xx", &x, &y);
        if (r < 0) {
                fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
                return r;
        }

        /* Return an error on division by zero */
        if (y == 0) {
                sd_bus_error_set_const(ret_error, "net.poettering.DivisionByZero", "Sorry, can't allow division by zero.");
                return -EINVAL;
        }

        return sd_bus_reply_method_return(m, "x", x / y);
}

static const sd_bus_vtable tukit_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD_WITH_ARGS("update", SD_BUS_NO_ARGS, SD_BUS_RESULT("i", ret), method_multiply, 0),
        SD_BUS_VTABLE_END
};

int main(int argc, char *argv[]) {
        sd_bus_slot *slot = NULL;
        sd_bus *bus = NULL;
        int r;

        r = sd_bus_open_system(&bus);
        if (r < 0) {
                fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
                goto finish;
        }

        r = sd_bus_add_object_vtable(bus,
                                     &slot, // Muss vermutlich non-floating sein
                                     "/org/opensuse/tukit",
                                     "org.opensuse.tukit",
                                     tukit_vtable,
                                     NULL);
        if (r < 0) {
                fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
                goto finish;
        }

        /* Take a well-known service name so that clients can find us */
        r = sd_bus_request_name(bus, "org.opensuse.tukit", 0);
        if (r < 0) {
                fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
                goto finish;
        }

        for (;;) {
                /* Process requests */
                r = sd_bus_process(bus, NULL);
                if (r < 0) {
                        fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
                        goto finish;
                }
                if (r > 0) /* we processed a request, try to process another one, right-away */
                        continue;

                /* Wait for the next request to process */
                r = sd_bus_wait(bus, (uint64_t) -1);
                if (r < 0) {
                        fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
                        goto finish;
                }
        }

finish:
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

