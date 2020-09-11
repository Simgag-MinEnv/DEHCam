#particle flash --usb `basename $1`.bin
particle list | grep online | grep $1

#particle flash $1 `basename $2`.bin