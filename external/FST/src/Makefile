subdirs=FstHost FstPlugin JstHost JstPlugin

.PHONY: default
default:
	for i in $(subdirs); do make -C $$i; done

%:
	for i in $(subdirs); do make -C $$i $@; done

