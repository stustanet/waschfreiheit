gcc -shared -Wl,-soname,node_pytest.so -o node_pytest.so  -DENABLE_ASSERT -I../header ../state_estimation.c se_pytester.c
