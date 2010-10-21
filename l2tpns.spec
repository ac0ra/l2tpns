# SVN Particulars
%define svn_url "http://build.dev.iseek.com.au/development/internal/software/l2tpns/trunk"

# SVN Version
%define svn_revision 3122

# Release counter
%define rpm_release 1.freetraffic.beta


Summary: A high-speed clustered L2TP LNS
Name: l2tpns
Version: 2.1.21
Release: %svn_revision.%rpm_release
License: GPL
Group: System Environment/Daemons
#source: http://optusnet.dl.sourceforge.net/sourceforge/l2tpns/l2tpns-%{version}.tar.gz
URL: http://sourceforge.net/projects/l2tpns
BuildRoot: %{_tmppath}/%{name}-%{svn_revision}-root
Prereq: /sbin/chkconfig
BuildRequires: libcli >= 1.8.5, subversion, libcli-devel
Requires: libcli >= 1.8.5

%description
l2tpns is a layer 2 tunneling protocol network server (LNS).  It
supports up to 65535 concurrent sessions per server/cluster plus ISP
features such as rate limiting, walled garden, usage accounting, and
more.

%prep

# Blow away old buildroot
rm -fr %{buildroot}-svn
# Create base directories
mkdir -p %{buildroot}-svn
# Export this revision
svn -r %{svn_revision} export %{svn_url} %{buildroot}-svn/l2tpns

%build
cd %{buildroot}-svn/l2tpns
make
# Was once : 
# make ISEEK_CONTROL_MESSAGE=1

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cd %{buildroot}-svn/l2tpns
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}
rm -rf %{buildroot}-svn

%files
%defattr(-,root,root)
#%doc Changes INSTALL INTERNALS COPYING THANKS Docs/manual.html
%dir /etc/l2tpns
%dir /var/run/l2tpns/acct
%dir /var/run/l2tpns/gardenacct
%config(noreplace) /etc/l2tpns/users
%config(noreplace) /etc/l2tpns/startup-config
%config(noreplace) /etc/l2tpns/ip_pool
%config(noreplace) /etc/l2tpns/free_networks
%attr(755,root,root) /usr/sbin/*
%attr(755,root,root) /usr/lib/l2tpns
%attr(644,root,root) /usr/share/man/man[58]/*

%changelog
* Mon Jul  5 2010 Robert McLeay <robert@iseek.com.au> 2.1.21-1.3011
- Made CentOS 5 compliant.
- Merged Jacob's requests for Walled Garden Name and throttle speed.
- Fixed requirement for manual creation of accounting directories.

* Tue Jun  1 2010 Thomas Guthmann <tguthmann@iseek.com.au> 2.1.21-1.2981
- Fixed default garden name to 'garden' instead of NULL
- Added set default_garden option
- Added more logs 
- Added a comment in spec file to remember ISEEK_CONTROL_MESSAGE=1

* Mon Oct 26 2009 Thomas Guthmann <tguthmann@iseek.com.au> 2.1.21-1.2243
- Includes pool details (show pool summary)
- Includes pool loading from disk (add ip-pool 1.2.3.4/25 MI)

* Mon Jun 22 2009 Robert McLeay <robert@iseek.com.au> 2.1.21-1.2065
- Added more diagnostics to provide information on pool name for show session.

* Mon Jun 22 2009 Robert McLeay <robert@iseek.com.au> 2.1.21-1.2059
- Rebuild to fix the multiple-IP pool crash-on-disconnect
- Removed a compile warning
- Fixed the Makefile to build correctly under the mach system.

* Thu May  8 2008 Stuart Low <stuart@iseek.com.au> 2.1.21-1.959
- Rebuild for VMA dynamic preps
- Includes minor bug fixes & log level changes

* Wed Jun 13 2007 Robert McLeay <robert@iseek.com.au> 2.1.21-1
- Reroll with new tarball + iseek patches
* Fri Aug 4 2006 Stuart Low <stuart@iseek.com.au> 2.1.20-1
- Reroll of 2.1.19 with 2.1.20 tarball.
- Includes some vendor specific attribute bug fixes
* Fri Jun 23 2006 Brendan O'Dea <bod@optus.net> 2.1.19-1
- 2.1.19 release, see /usr/share/doc/l2tpns-2.1.19/Changes
