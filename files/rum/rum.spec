Name:           rum
Version:        1.0.0
Release:        1%{?dist}
Summary:        rum is a rug-like interface for yum 

Group:          Applications/System
License:        GPL
URL:            http://code.google.com/p/rum
Source0:        rum-1.0.0.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:      noarch

BuildRequires:  python
Requires:       yum >= 2.9.5

%description
rum is a rug-like interface for yum (Yellow dog Updater, Modified).  rug is the command-line frontend for rcd (Red Carpet Daemon).

%prep
%setup -q


%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%defattr(-,root,root)
%{_bindir}/rum
%{_datadir}/rum/*

%doc README AUTHORS COPYING INSTALL

%changelog
* Wed Sep 6 2006 James Willcox <snorp@snorp.net> - 1.0.0-1
- Initial package
