all: compile

buildrpc:
	rpcgen -N -M -C -L -h -o l2tp_rpc.h l2tp_rpc.x
	rpcgen -N -M -C -L -c -o l2tp_rpc_xdr.c l2tp_rpc.x
	rpcgen -N -M -C -L -l -o l2tp_rpc_client.c l2tp_rpc.x

compile:
	gcc -o l2tp_rpc_client *.c

veryclean:
	rm -f *.o l2tp_rpc.h l2tp_rpc_xdr.c l2tp_rpc_client.c

clean:
	rm -f *.o
