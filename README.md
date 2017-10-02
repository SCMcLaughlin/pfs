# pfs

Command line tool for working with Everquest's data file archives in the PFS format.

## Usage

```sh
pfs [OPTIONS] [FILE]
```

## Examples

To list the contents of a PFS file (gfaydark.s3d, in this example):

```sh
pfs -l gfaydark.s3d
```

Extracting a file (nekpine.bmp) into the current working directory:

```sh
pfs -e nekpine.bmp gfaydark.s3d
```

Inserting a file (example.txt) into an existing PFS archive:

```sh
pfs -i /path/to/example.txt gfaydark.s3d
```

Removing files from a PFS archive:

```sh
pfs -r example.txt nekpine.bmp gfaydark.s3d
```

Creating a new, empty PFS archive:

```sh
pfs -c example.s3d
```

Transferring a file from one PFS archive to another (requires shell pipe support)

```sh
pfs -o nekpine.bmp gfaydark.s3d | pfs -w tree.bmp anguish.eqg
```

