## Setup Env 
### 1.0. Install boost 
```
brew install boost
ln -s installed_boost /usr/local/Cellar/boost
```
### 1.1. Set LLDB Headers
```
sudo cp -r lldbHeaders /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Versions/A/Headers
ln -s /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Versions/Current/Headers /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Headers
```
