## Automatic build & release
This repository supports automatic release creation base on pushed tags. 
The type of release depends on tag value. 
Tag value should match one of the following regular expressions:

- `[0-9]+.[0-9]+.[0-9]+` - normal release

- `[0-9]+.[0-9]+.[0-9]+-[a-zA-Z]+` - draft release (visible only to project maintainers)

- `[a-zA-Z]+-[0-9]+.[0-9]+.[0-9]+` - pre-release (visible to all)
 
