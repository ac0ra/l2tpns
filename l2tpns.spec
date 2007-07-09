# SVN Particulars
%define svn_url "http://dev.iseek.com.au/development/internal/software/l2tpns"

# SVN Version
%define svn_revision 219

# Release counter
%define rpm_release 1


Summary: A high-speed clustered L2TP LNS
Name: l2tpns
Version: 2.1.21
Release: %svn_revision.%rpm_release
License: GPL
Group: System Environment/Daemons
Source: http://optusnet.dl.sourceforge.net/sourceforge/l2tpns/l2tpns-%{version}.tar.gz
URL: http://sourceforge.net/projects/l2tpns
BuildRoot: %{_tmppath}/%{name}-%{svn_revision}-root
Prereq: /sbin/chkconfig
BuildRequires: libcli >= 1.8.5
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
%config(noreplace) /etc/l2tpns/users
%config(noreplace) /etc/l2tpns/startup-config
%config(noreplace) /etc/l2tpns/ip_pool
%attr(755,root,root) /usr/sbin/*
%attr(755,root,root) /usr/lib/l2tpns
%attr(644,root,root) /usr/share/man/man[58]/*

%changelog
* Wed Jun 13 2007 Robert McLeay <robert@iseek.com.au> 2.1.21-1
- Reroll with new tarball + iseek patches
* Fri Aug 4 2006 Stuart Low <stuart@iseek.com.au> 2.1.20-1
- Reroll of 2.1.19 with 2.1.20 tarball.
- Includes some vendor specific attribute bug fixes
* Fri Jun 23 2006 Brendan O'Dea <bod@optus.net> 2.1.19-1
- 2.1.19 release, see /usr/share/doc/l2tpns-2.1.19/Changes
