export default defineAppConfig({
    docus: {
        locale: 'en',
        colorMode: 'light',
    },
    navigation: {
        sub: 'header',
    },
    github: {
        url: 'https://github.com/XCSoar/XCSoar',
        branch: 'master',
        rootDir: 'docs',
    },
    ui: {
        prose: {
            h2: {
                slots: {
                    base: ['text-3xl'],
                    link: 'text-primary-500',
                },
            },
        },
    },
});
