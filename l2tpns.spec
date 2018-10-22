# SVN Particulars
%define svn_url "http://build.dev.iseek.com.au/development/internal/software/l2tpns/trunk"

# SVN Version
%define svn_revision 3122

# Release counter
%define rpm_release 1.freetraffic.beta


Summary: A high-speed clustered L2TP LNS
Name: l2tpns
Version: 2.2.0
Release: 1
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
* Mon Dec 18 2006 Brendan O'Dea <bod@optus.net> 2.2.0-1
- 2.2.0 release, see /usr/share/doc/l2tpns-2.2.0/Changes
