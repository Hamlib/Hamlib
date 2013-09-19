#!/usr/bin/awk -f

# (c) 2007-2009 Stephane Fillod

#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# This program is to be piped from:
#  tests/rigctl -l |sort -n | rig_split_lst.awk

BEGIN {
	bkendlst[0]="dummy"
	bkendlst[1]="yaesu"
	bkendlst[2]="kenwood"
	bkendlst[3]="icom"
	bkendlst[4]="pcr"
	bkendlst[5]="aor"
	bkendlst[6]="jrc"
	bkendlst[7]="radioshack"
	bkendlst[8]="uniden"
	bkendlst[9]="drake"
	bkendlst[10]="lowe"
	bkendlst[11]="racal"
	bkendlst[12]="wj"
	bkendlst[13]="ek"
	bkendlst[14]="skanti"
	bkendlst[15]="winradio"
	bkendlst[16]="tentec"
	bkendlst[17]="alinco"
	bkendlst[18]="kachina"
	bkendlst[20]="gnuradio"
	bkendlst[21]="microtune"
	bkendlst[22]="tapr"
	bkendlst[23]="flexradio"
	bkendlst[24]="rft"
	bkendlst[25]="kit"
	bkendlst[26]="tuner"
	bkendlst[27]="rs"
	bkendlst[28]="unknown"
	if (lst_dir == "")
		lst_dir="."
	else
		lst_dir=lst_dir


	for (bke in bkendlst) {
		print " <html> <head> <META http-equiv=\"content-type\" " \
		      "content=\"text/html;charset=iso-8859-1\">" \
			" <title>Hamlib rig matrix</title></head> <body><p>" \
			"<center><table>" \
			"<tr><td>Rig#</td><td>Mfg</td><td>Model</td>" \
			"<td>Vers.</td><td>Status</td></tr>" > lst_dir"/"bkendlst[bke]"_lst.html"
	}
}

# 0         1         2         3         4         5
# 012345678901234567890123456789012345678901234567890123456789
# 2004    GNU             GNU Radio GrAudio I&Q   0.1.2   New

$1 ~ /^[0-9]+$/ {
	num=$1
	mfg=$2
	sub("^[0-9]+\t","")
	model=substr($0,17,24)
	ver=substr($0,41,8)
	status=substr($0,49)

	printf "<tr><td><a href=\"support/model%d.txt\">%d</a></td>" \
	       "<td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
		num,num,mfg,model,ver,status >> lst_dir"/"bkendlst[int(num/100)]"_lst.html"
}

END {
	for (bke in bkendlst) {
		print "</table></center></body></html>" >> lst_dir"/"bkendlst[bke]"_lst.html"
	}
}
