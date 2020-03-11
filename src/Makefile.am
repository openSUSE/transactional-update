AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS=transactional-update
transactional_update_SOURCES=main.cpp \
        TransactionalUpdate.cpp Transaction.cpp \
        TransactionalCommand.cpp \
        Commands/Cleanup.cpp Commands/DistUpgrade.cpp Commands/Update.cpp \
        Snapshot.cpp Snapshot/Snapper.cpp \
        Mount.cpp \
        Util.cpp Configuration.cpp
noinst_HEADERS=TransactionalUpdate.h \
        Transaction.h TransactionalCommand.h Command.h \
        Commands/Cleanup.h Commands/DistUpgrade.h Commands/Update.h \
        Snapshot.h Snapshot/Snapper.h \
        Mount.h \
        Util.h Configuration.h Exceptions.h
transactional_update_CPPFLAGS=$(ECONF_CFLAGS)
transactional_update_LDFLAGS=$(ECONF_LIBS) -lmount
