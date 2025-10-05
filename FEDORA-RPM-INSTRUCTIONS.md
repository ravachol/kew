# Building an RPM package

For RPM-based distributions (like Fedora, CentOS, RHEL), you can build the package from source using the provided `.spec` file.

1.  **Install Build Tools & Dependencies**

    First, install the necessary build dependencies for `kew` by following the instructions for your distribution (e.g., Fedora) in the "Building the project manually" section.

    Then, install the RPM build tools:
    ```bash
    sudo dnf install rpm-build
    ```

2.  **Prepare Source Tarball**

    Create the source tarball from the git repository and place it where `rpmbuild` can find it:
    ```bash
    # Define the version based on the spec file
    VERSION=$(grep 'Version:' kew.spec | awk '{print $2}')

    # Create the rpmbuild directory structure
    mkdir -p ~/rpmbuild/SOURCES

    # Create the source tarball
    git archive --format=tar.gz --prefix=kew-$VERSION/ -o ~/rpmbuild/SOURCES/kew-$VERSION.tar.gz HEAD
    ```

3.  **Build the RPM**

    Now, you can build the binary and source RPMs:
    ```bash
    rpmbuild -ba kew.spec
    ```
    The resulting RPM files will be created in the `~/rpmbuild/RPMS/` and `~/rpmbuild/SRPMS/` directories.