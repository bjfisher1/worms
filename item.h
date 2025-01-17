/**
    @file item.h
    @author Ben Fisher

    Defines the structure for items, such as crates and health boxes.
*/
#ifndef ITEM_H
#define ITEM_H
#include "physObj.h"
#include "worm.h"
#include "level.h"
#include "weapon.h"
#include "anim.h"
#include "level.h"
#include <stdbool.h>
#include <time.h>
#define ADD_BACK_NO_ITEMS 0
#define ADD_BACK_FIRST_ITEM 1
#define ADD_BACK_SECOND_ITEM 2
#define ADD_BACK_BOTH_ITEMS 3

typedef enum {
    healthCrateItem,
    weaponCrateItem,
    mineItem,
    dynamiteItem,
    bulletItem
} ItemName;

/**
    An item has a physical object component, a current animation, and
    a function for if it collides with a worm in the level.
*/
typedef struct ItemTag {
    PhysObj *obj;
    Anim *currentAnim;
    bool (*wormCollide)(void *this, Worm *worm, void *game);
    int (*itemCollide)(void *this, void *other, void *game);
    int explosionRadius;
    Anim *anim;
    int animFrame;
    void (*free)(void *self);
    void (*act)(void *self);
    ItemName name;
    int id;
    clock_t lastPlayed;
} Item;

/**
    Structure subclass items are cast to to access the item aspect of them.
*/
typedef struct {
    Item *item;
} ItemSubclass;

/**
    Structure for health crate.
*/
typedef struct {
    Item *item;
    int healAmount;
} HealthCrate;

/**
    Structure for a weapon crate.
*/
typedef struct {
    Item *item;
    bool isTrap;
    Weapon *weapon;
} WeaponCrate;

/**
    Structure for a mine.
*/
typedef struct {
    Item *item;
    clock_t start;
    float delay;
    bool triggered;
} Mine;

/**
    Structure for a dynamite item.
*/
typedef struct {
    Item *item;
    clock_t start;
    float delay;
} Dynamite;

typedef struct {
    Item *item;
    int startx;
    int starty;
    int range;
} Bullet;

/**
    Creates a health crate with coordinates (x,y) and an explosion
    radius.

    @param x the start x coordinate of the crate
    @param y the start y coordinate of the crate
    @param explosionRadius the radius of the explosion if the crate
           explodes
    
    @return the created health crate
*/
HealthCrate *createHealthCrate(int x, int y, int explosionRadius, int healAmount, void *game);

/**
    Creates a health crate at coordinates (x,y), an explosion
    radius, and the weapon inside.

    @param x the start x coordinate of the crate
    @param y the start y coordinate of the crate
    @param explosionRadius the radius of the explosion if the crate
           explodes
    
    @return the created weapon crate
*/
WeaponCrate *createWeaponCrate(int x, int y, int explosionRadius, Weapon *weapon, bool isTrap, void *game);

/**
    Creates a mine at (x,y) with the given explosion radius.

    @param x the start x coordinate of the mine
    @param y the start y coordinate of the mine
    @param explosionRadius the radius of the explosion if the mine
           explodes
    
    @return the created mine
*/
Mine *createMine(int x, int y, int explosionRadius, clock_t start, float delay, void *game);

/**
    Frees the health crate.

    @param healthCrate the crate being freed
*/
void freeHealthCrate(void *healthCrate);

/**
    Frees the weapon crate.

    @param weaponCrate the crate being freed
*/
void freeWeaponCrate(void *weaponCrate);

void drawItem(Item *item);

void clearItem(Item *item, Level *level);

Dynamite *createDynamite(int x, int y, int explosionRadius, float delay, void *game);

void freeDynamite(void *dynamite);

bool dynamiteReadyToExplode(void *dynamite);

bool mineReadyToExplode(Mine *mine);

bool bulletOutOfRange(Bullet *bullet);

Bullet *createBullet(int x, int y, int explosionRadius, void *game, int range, float direction, float velocity);

void freeBullet(void *bullet);

#endif /* ITEM_H */