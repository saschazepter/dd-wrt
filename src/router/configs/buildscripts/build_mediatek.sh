#!/bin/sh
./build_whrg300n.sh
./build_whrg300n_openvpn.sh
./build_dir600.sh
./build_dir300b.sh
./build_dir615d.sh
./build_dir615h.sh
./build_esr9752sc.sh
./build_acxnr22.sh
./build_ar670w.sh
./build_ar690w.sh
./build_esr6650.sh
#./build_whrg300n_buffalo.sh
./build_ecb9750.sh
./build_br6574n.sh
./build_asus_rtn13u.sh
./build_asus_rtn13ub1.sh
./build_asus_rt10n_plus.sh
./build_wr5422.sh
./build_eap9550.sh
./build_f5d8235.sh
./build_asus_rt15n.sh
./build_wcrgn.sh
./build_w502u.sh
./build_whr300hp2.sh
./build_whr1166d.sh
./build_e1700.sh
./build_dir810l.sh
./build_dir860l.sh
./build_dir882.sh
cd broadcom_2_6_80211ac_mipselr1/opt 
./do_eko_mega_v24-K26-nv64k.sh
cd ../../

chmod -R 777 /GruppenLW/releases
DATE=$(date +%m-%d-%Y)
DATE+="-r"
DATE+=$(svnversion -n ar531x/src/router/httpd)
scp -r /GruppenLW/releases/$DATE ftp.dd-wrt.com:/downloads
