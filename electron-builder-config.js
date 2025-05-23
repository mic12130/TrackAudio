module.exports = {
  appId: 'com.vatsim.trackaudio',
  productName: 'TrackAudio',
  directories: {
    buildResources: 'build'
  },
  publish: {
    provider: 'github',
    owner: 'pierr3',
    repo: 'TrackAudio',
    releaseType: 'draft',
    vPrefixedTagName: false
  },
  files: [
    '!**/.vscode/*',
    '!src/*',
    '!electron.vite.config.{js,ts,mjs,cjs}',
    '!{.eslintignore,.eslintrc.cjs,.prettierignore,.prettierrc.yaml,dev-app-update.yml,CHANGELOG.md,README.md}',
    '!{.env,.env.*,.npmrc,pnpm-lock.yaml}',
    '!{.gitignore,.gitmodules,.sentryclirc,MANDATORY_VERSION}',
    '!{tsconfig.json,tsconfig.node.json,tsconfig.web.json}',
    '!backend/*'
  ],
  asarUnpack: ['resources/**', './src/renderer/src/assets/md80_error.mp3'],
  win: {
    executableName: 'trackaudio',
    extraFiles: [
      {
        from: 'backend/build/Release/',
        to: '.',
        filter: ['*.dll']
      },
      {
        from: 'resources/',
        to: 'resources/',
        filter: ['*.wav']
      }
    ]
  },
  nsis: {
    artifactName: '${name}-${version}-${arch}-setup.${ext}',
    shortcutName: '${productName}',
    uninstallDisplayName: '${productName}',
    createDesktopShortcut: 'always',
    include: 'build/installer.nsh'
  },
  mac: {
    entitlementsInherit: 'build/entitlements.mac.plist',
    notarize: true,
    extraFiles: [
      {
        from: 'resources/',
        to: 'Resources/',
        filter: ['*.wav']
      }
    ]
  },
  dmg: {
    artifactName: '${name}-${version}-${arch}.${ext}'
  },
  linux: {
    target: ['AppImage', 'snap', 'deb'],
    maintainer: 'pierr3',
    category: 'Game',
    extraFiles: [
      {
        from: 'resources/',
        to: 'resources/',
        filter: ['*.wav']
      }
    ]
  },
  appImage: {
    artifactName: '${name}-${version}-${arch}.${ext}'
  },
  npmRebuild: false,
  electronDownload: {
    mirror: 'https://npmmirror.com/mirrors/electron/'
  }
};
