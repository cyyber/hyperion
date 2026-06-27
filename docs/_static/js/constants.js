// Site URL
const SITE_URL = ""
const { origin, pathname } = location;
const pathSplit = pathname.split("/");
const rootPath = SITE_URL && origin.includes(SITE_URL) && pathSplit.length > 3 ? pathSplit.splice(1, 2).join("/") : ''
const ROOT_URL = `${origin}/${rootPath}`;

// Color mode constants
const [DARK, LIGHT] = ["dark", "light"];
const LIGHT_LOGO_PATH = `${ROOT_URL}/_static/img/logo.svg`;
const DARK_LOGO_PATH = `${ROOT_URL}/_static/img/logo-dark.svg`;
const SUN_ICON_PATH = `${ROOT_URL}/_static/img/sun.svg`;
const MOON_ICON_PATH = `${ROOT_URL}/_static/img/moon.svg`;
const LIGHT_HAMBURGER_PATH = `${ROOT_URL}/_static/img/hamburger-light.svg`;
const DARK_HAMBURGER_PATH = `${ROOT_URL}/_static/img/hamburger-dark.svg`;
const COLOR_TOGGLE_ICON_CLASS = "color-toggle-icon";
const HYPERION_LOGO_CLASS = "hyperion-logo";
const LS_COLOR_SCHEME = "color-scheme";

// Hyperion navigation constants
const HYPERION_HOME_URL = "https://github.com/theQRL/hyperion";
const DOCS_URL = "/";
const EXAMPLES_PATH = `/en/latest/hyperion-by-example.html`;
const CONTRIBUTE_PATH = `/en/latest/contributing.html`;
const QRL_DEVELOPERS_PATH = "https://theqrl.org/en/developers/";
const NAV_LINKS = [
  { name: "Source", href: HYPERION_HOME_URL },
  { name: "Documentation", href: DOCS_URL },
  { name: "Examples", href: EXAMPLES_PATH },
  { name: "Contribute", href: CONTRIBUTE_PATH },
  { name: "QRL Developers", href: QRL_DEVELOPERS_PATH },
];

const MOBILE_MENU_TOGGLE_CLASS = "shift";
const WRAPPER_CLASS = "unified-wrapper";
