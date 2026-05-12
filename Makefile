.PHONY: all clean

MODULES := queue threads vm fs

all:
	@for m in $(MODULES); do $(MAKE) -C $$m || exit 1; done

clean:
	@for m in $(MODULES); do $(MAKE) -C $$m clean; done
