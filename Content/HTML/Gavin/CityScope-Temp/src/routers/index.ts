import { createRouter, createWebHistory } from "vue-router";

const routers = [
    {
        path: "/",
        name: "Home",
        component: () => import("@/views/Home.vue")
    },
    {
        path: "/home",
        redirect: { name: "Home" }
    },
    {
        path: "/admin-div-visual",
        name: "AdminDivVisual",
        component: () => import("@/views/AdminDivVisual.vue")
    },
    {
        path: "/precinct-visual",
        name: "PrecinctVisual",
        component: () => import("@/views/PrecinctVisual.vue")
    },
    {
        path: "/precinct-add",
        name: "PrecinctAdd",
        component: () => import("@/views/PrecinctAdd.vue")
    },
    {
        path: "/precinct-modify",
        name: "PrecinctModify",
        component: () => import("@/views/PrecinctModify.vue")
    },
    {
        path: "/precinct-delete",
        name: "PrecinctDelete",
        component: () => import("@/views/PrecinctDelete.vue")
    },
    {
        path: "/subdistrict-modify" ,
        name: "SubdistrictModify",
        component: () => import("@/views/SubdistrictModify.vue")
    }
];

const router = createRouter({
    history: createWebHistory(),
    routes: routers
});

export default router;