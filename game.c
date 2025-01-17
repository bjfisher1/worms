#include "game.h"
#include <stdlib.h>
#include "util.h"
#include <math.h>
#include "stamp.h"
#include "List/ArrayList.h"
#define WIDTH 1920
#define HEIGHT 696
#define DEPTH 32
#define TERMINAL_FALL_VELOCITY 25.0
#define REFRESH_RATE .001
#define X_ACCEL .2
#define MAX_X_VELOCITY 5.0
#define JUMP 7.0
#define MAX_JUMP_VELOCITY 25
#define WORM_SPEED 2
#define WEAPON_TURN_SPEED (PI/90.0)
#define MAX_WEAPON_TURN (PI/2.0)
#define WEAPON_FORCE_MAX 100
#define WEAPON_FORCE_DELTA 4
#define WEAPON_FORCE_NON_RANGED 70
#define WEAPON_FORCE_VEL_RATIO .1
#define WORM_BOUNCE 0.0
#define ITEM_BOUNCE 0.3
#define INV_CELLS_HIGH 2
#define INV_CELLS_WIDE 5
#define NAME_HEIGHT 60
#define TIMER_HEIGHT 90
#define WEAPON_AMOUNT_MAX 5
#define START_TEAM_IDX 2
#define BIG_EXPLOSION_SIZE 50
#define EXPLOSION_DAMAGE_SCALE 2.5
#define DEATH_BORDER 50
#define DEATH_EXPLOSION 30
#define TURN_DELAY 1

bool wormOffGrid(Worm *worm);
void setGameSelectedWeapon(Game *game, int weaponIdx);
void dropCrates(Game *game, int crateNum);
void switchTeam(Game *game);
void switchPlayer(Game *game);
bool isStill(Game *game);
float timeLeftInTurn(Game *game);
void drawTimeLeftInTurn(Game *game);
void clearWormName(Game *game, Worm *worm);
void drawWormName(Game *game, Worm *worm);

void explodeWorms(Queue *teams, Game *game, int x, int y, int radius, float maxVelocity);

void drawInventory(Game *game, Team *team);
void clearKeys(bool *left, bool *right, bool *up,
              bool *down, bool *space, bool *tab, bool *esc, bool *enter,
              bool *backspace, bool *one, bool *two, bool *three, bool *four, bool *five);
void readKeys(SDL_Event *event, bool *left, bool *right, bool *up,
              bool *down, bool *space, bool *tab, bool *esc, bool *enter,
              bool *backspace, bool *one, bool *two, bool *three, bool *four, bool *five);

Game *startGame(Level *level, Queue *teams, int turnLength, float gravity) {
    SDL_Surface *screen;
    if (SDL_Init(SDL_INIT_VIDEO) < 0 ) exit(EXIT_FAILURE);
   
    if (!(screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_RESIZABLE|SDL_HWSURFACE)))
    {
        SDL_Quit();
        exit(EXIT_FAILURE);
    }
    
    int animBankLen;
    Anim **animBank = loadAnims(screen, &animBankLen);

    if(!animBank) {
        SDL_Quit();
        fprintf(stderr, "Couldn't load animation\n");
        exit(EXIT_FAILURE);
    }

    
    Game *game = (Game *) malloc(sizeof(Game));
    game->level = level;
    game->teams = teams;
    game->items = makeQueue();
    game->stamps = makeQueue();
    game->animBank = animBank;
    game->animNumber = animBankLen;
    game->gravity = gravity;
    game->turnLength = turnLength;
    game->teamCanSwitchMember = false;
    if(!game->animBank) {
        fprintf(stderr, "Couldn't load animation file(s).\n");
        exit(EXIT_FAILURE);
    }
    game->screen = screen;
    Color white = {255, 255, 255};
    Color black = {0, 0, 0};
    game->font = loadFont("anims/font.ppm", game->screen, white, black, 1);
    if(!game->font) {
        fprintf(stderr, "Couldn't load font file.\n");
        exit(EXIT_FAILURE);
    }
    Team *team;
    for(int i = 0; i < queueSize(game->teams); i++) {
        team = dequeue(game->teams);
        enqueue(game->teams, team);
        for(int j = 0; j < team->teamNumber; j++) {
            switchAnim(team->worms[j], game->animBank[wormStill]);
        }
    }
    for(int i = 0; i < START_TEAM_IDX; i++){
        game->currentTeam = dequeue(game->teams);
        enqueue(game->teams, game->currentTeam);
    }

    game->player = team->worms[0];
    game->lastUpdate = clock();
    game->turnStart = clock();
    game->waitForTurn = true;
    game->lastTurn = clock();
    return game;
}

void moveObjectsInGame(Game *game);

bool wormsHaveSettled(Game *game);

void endGame(Game *game) {
    freeAnims(game->animBank, game->animNumber);
    SDL_Quit();
    freeLevel(game->level);
    int teamNumber = queueSize(game->teams);
    for(int i = 0; i < teamNumber; i++) {
        Team *team = dequeue(game->teams);
        freeTeam(team);
    }
    int itemNumber = queueSize(game->items);
    for(int i = 0; i < itemNumber; i++) {
        ItemSubclass *item = dequeue(game->items);
        item->item->free(item);
    }
    //FREE STAMPS TODO
    freeQueue(game->items);
    freeQueue(game->teams);
    freeFont(game->font);
    free(game);
}

//NEED TO ADD WORM MAX DIST
bool gameLoop(Game *game) {
    static SDL_Event event;
    static bool left, right, up, down, space, tab, enter, backspace, esc, one, two, three, four, five, le, ri, to, bo, anyCol;
    static bool firing = false;
    static int mousex, mousey, teamNumber, wormNumber, stampNumber, itemNumber, weaponFrame;
    static int fps = 0;
    static float weaponDir = 0.0;
    static float weaponDirTopAngle = 0.0;
    static float weaponForce = 1;
    static float weaponForceDelta = WEAPON_FORCE_DELTA;
    static Worm *worm;
    static Team *team;
    static Weapon *currentWeapon;
    static clock_t lastSec = 0;
    readKeys(&event, &left, &right, &up, &down, &space, &tab, &esc, &enter, &backspace, &one, &two, &three, &four, &five);
    
    if(esc) {
        return true;
    }

    if(game->waitForTurn) {
        clearKeys(&left, &right, &up, &down, &space, &tab, &esc, &enter, &backspace, &one, &two, &three, &four, &five);
        if(( (float) (clock() - game->lastTurn)) / CLOCKS_PER_SEC >= TURN_DELAY){
            game->waitForTurn = false;
            game->turnStart = clock();
        }
    }

    SDL_GetMouseState(&mousex, &mousey);

    teamNumber = queueSize(game->teams);

    if( ( (float) (clock() - lastSec)) / CLOCKS_PER_SEC >= 1.0 ) {
        
        lastSec = clock();
        printf("%d \n", fps);
        fps = 0;
    }
    
    if( ( (float) (clock() - game->lastUpdate)) / CLOCKS_PER_SEC >= REFRESH_RATE ) {
        fps++;
        //the current team's weapon
        currentWeapon = &( (InvWeapon *) getArrayListElement(game->currentTeam->weapons, game->currentTeam->selectedWeapon))->weapon;

        if(tab) {
            tab = false;
            switchPlayer(game);
        }
        game->player = game->currentTeam->worms[game->currentTeam->playerIdx];

        isColliding(game->player->obj, game->level, &le, &ri, &to, &bo);
        if(right) {
            switchAnim(game->player, game->animBank[wormMove]);
            if(to){
                accel(game->player->obj, game->player->obj->rotation, WORM_SPEED, WORM_SPEED);
            } else {
                game->player->obj->x += WORM_SPEED / 2.0;
            }
            //printf("%f\n", worm->obj->rotation);
            
            //moveRight(worm->obj,level, 3);
            flipWormRight(game->player);
        }
        if(left) {
            switchAnim(game->player, game->animBank[wormMove]);
            //accel(worm->obj, PI, X_ACCEL, MAX_X_VELOCITY);
            if(to) {
                accel(game->player->obj, game->player->obj->rotation + PI, WORM_SPEED, WORM_SPEED);
            } else {
                game->player->obj->x -= WORM_SPEED / 2.0;
            }
            
            //moveLeft(worm->obj,level, 3);
            flipWormLeft(game->player);
        }
        if(backspace && to) {
            accel(game->player->obj,  -PI/2.0, JUMP, MAX_JUMP_VELOCITY);
        }
        /*
        else if(enter && to) {
            float angleFromTop = PI/4.0;
            if(game->player->facingRight) {
                angleFromTop *= -1;
            }
            accel(game->player->obj,  -PI/2.0 - angleFromTop, JUMP, MAX_JUMP_VELOCITY);
        }
        */
        if(!right && !left) {
            decel(game->player->obj, X_ACCEL);
            switchAnim(game->player, game->animBank[wormStill]);
            if(game->player->facingRight) {
                flipWormRight(game->player);
            } else {
                flipWormLeft(game->player);
            }
        }
        
        if(up && weaponDirTopAngle >= -MAX_WEAPON_TURN) {
            weaponDirTopAngle -= WEAPON_TURN_SPEED;
        }
        if(down && weaponDirTopAngle <= MAX_WEAPON_TURN){
            weaponDirTopAngle += WEAPON_TURN_SPEED;
        }
        //weaponDir = weaponDirTopAngle;
        if(game->player->facingRight) {
            weaponDir = weaponDirTopAngle;
        } else {
            weaponDir = -weaponDirTopAngle;
        }
        float weaponFireDir = weaponDir;
        if(!game->player->facingRight) {
            weaponFireDir += PI;
        }

        if(space) {
            if(!firing) {
                weaponForce = 0;
            }
            firing = true;
        }

        if(firing) {
            if(currentWeapon->rangedAttack) {
                weaponForce += weaponForceDelta;
                if(weaponForce >= WEAPON_FORCE_MAX) {
                    weaponForce = WEAPON_FORCE_MAX;
                    weaponForceDelta = -abs(weaponForceDelta);
                }
                else if(weaponForce <= 0) {
                    weaponForce = 0;
                    weaponForceDelta = abs(weaponForceDelta);
                }
            } else {
                weaponForce = WEAPON_FORCE_NON_RANGED;
            }
            
            if(!currentWeapon->fireAtRelease || (currentWeapon->fireAtRelease && !space)) {
                fireWeapon(currentWeapon->name,
                           (void *) game, game->player->obj->x, game->player->obj->y,
                           weaponFireDir, weaponForce * WEAPON_FORCE_VEL_RATIO);
                firing = false;
                ( (InvWeapon *) getArrayListElement(team->weapons, game->currentTeam->selectedWeapon))->amount--;
            }
        } else {
            weaponForce = WEAPON_FORCE_NON_RANGED;
        }

        if(one) {
            setGameSelectedWeapon(game, 0);
        }

        if(two) {
            setGameSelectedWeapon(game, 1);
        }

        if(three) {
            setGameSelectedWeapon(game, 2);
        }

        if(four) {
            setGameSelectedWeapon(game, 3);
        }

        if(five) {
            setGameSelectedWeapon(game, 4);
        }

        //WORM AND ITEM COLLISIONS
        for(int i = 0; i < teamNumber; i++) {
            team = dequeue(game->teams);
            enqueue(game->teams, team);
            wormNumber = team->teamNumber;
            for(int j = 0; j < wormNumber; j++) {
                worm = team->worms[j];
                itemNumber = queueSize(game->items);
                for(int k = 0; k < itemNumber; k++) {
                    //do item stuff
                    void *subItem = dequeue(game->items);
                    bool addBackItem = true;
                    
                    Item *item = ((ItemSubclass *) subItem)->item;
                    if(areObjectsColliding(item->obj, worm->obj)) {
                        addBackItem = !item->wormCollide(((ItemSubclass *) subItem), worm, game);
                    }
                    if(addBackItem) {
                        enqueue(game->items, subItem);
                    }
                }
            }
        }

        //ITEM AND ITEM COLLISIONS
        itemNumber = queueSize(game->items);
        for(int i = 0; i < queueSize(game->items); i++) {
            void *subItem1 = dequeue(game->items);
            bool addBackItem1 = true;
            Item *item1 = ((ItemSubclass *) subItem1)->item;
            for(int j = i + 1; j < itemNumber; j++) {
                void *subItem2 = dequeue(game->items);
                bool addBackItem2 = true;
                Item *item2 = ((ItemSubclass *) subItem2)->item;
                if(areObjectsColliding(item1->obj, item2->obj)) {
                    int firstCol = item1->itemCollide(subItem1, subItem2, game);
                    int secCol = item2->itemCollide(subItem2, subItem1, game);
                    if(firstCol == ADD_BACK_NO_ITEMS) {
                        addBackItem1 = false;
                        addBackItem2 = false;
                    }
                    else if(firstCol == ADD_BACK_FIRST_ITEM) {
                        addBackItem2 = false;
                    } else if(firstCol == ADD_BACK_SECOND_ITEM) {
                        addBackItem1 = false;
                    }
                    if(secCol == ADD_BACK_NO_ITEMS) {
                        addBackItem1 = false;
                        addBackItem2 = false;
                    }
                    else if(secCol == ADD_BACK_FIRST_ITEM) {
                        addBackItem1 = false;
                    } else if(secCol == ADD_BACK_SECOND_ITEM) {
                        addBackItem2 = false;
                    }
                }
                if(addBackItem2) {
                    enqueue(game->items, subItem2);
                } else {
                    ((ItemSubclass *) subItem2)->item->free(subItem2);
                }
            }
            if(addBackItem1) {
                enqueue(game->items, subItem1);
            } else {
                ((ItemSubclass *) subItem1)->item->free(subItem1);
            }
        }
        

        //WORMS
        for(int i = 0; i < teamNumber; i++) {
            team = dequeue(game->teams);
            enqueue(game->teams, team);
            wormNumber = team->teamNumber;
            for(int j = 0; j < wormNumber; j++) {
                
                worm = team->worms[j];
                isColliding(worm->obj, game->level, &le, &ri, &to, &bo);
                if(!bo) {
                    accel(worm->obj, PI/2.0, game->gravity, TERMINAL_FALL_VELOCITY);
                }
                move(worm->obj, game->level, WORM_BOUNCE);
                tilt(worm->obj, game->level, 2 * PI/180.0, 45.0 * PI / 180.0,  game->screen );
                //DEATH
                if(worm->health <= 0 || wormOffGrid(worm)) {
                    char msg[100];
                    strcpy(msg, worm->name);
                    strcat(msg, " has died.");
                    writeText(game->font, msg, WIDTH / 2, HEIGHT / 2);
                    //clearWormName(game, worm);
                    Stamp *smokeStamp = createStamp(game->animBank[smokeAnim], 0, false, worm->obj->x, worm->obj->y);
                    createExplosion(game, worm->obj->x, worm->obj->y, DEATH_EXPLOSION);
                    enqueue(game->stamps, smokeStamp);
                    //if player has died, switch the team
                    if(worm == game->player) {
                        switchPlayer(game);
                        switchTeam(game);
                        return false;
                        //switchPlayer(game);
                    }
                    removeWormFromTeam(team, j);
                } else {
                    drawWorm(worm);
                    //drawWormName(game, worm);
                }
                
            }
        }

        //ITEMS
        itemNumber = queueSize(game->items);
        for(int i = 0; i < itemNumber; i++) {
            //do item stuff
            void *subItem = dequeue(game->items);
            
            Item *item = ((ItemSubclass *) subItem)->item;

            anyCol = isColliding(item->obj, game->level, &le, &ri, &to, &bo);
            if(!bo && item->name != bulletItem) {
                accel(item->obj, PI/2.0, game->gravity, TERMINAL_FALL_VELOCITY);
                move(item->obj, game->level, ITEM_BOUNCE);
            } else {
                ghostMove(item->obj);
            }
            drawItem(item);

            if(item->name == bulletItem) {
                Bullet *bullet = (Bullet *) subItem;
                if(bulletOutOfRange(bullet) || anyCol || bullet->item->obj->velocity < 1){
                    cutCircleInLevel( ((Game *) game)->level, item->obj->x, item->obj->y, bullet->item->explosionRadius );
                    clearItem(item, game->level);
                    freeBullet(bullet);
                } else {
                    enqueue(game->items, subItem);
                }
            }
            else if(item->name == dynamiteItem ) {
                Dynamite *dynamite = (Dynamite *) subItem;
                if(dynamiteReadyToExplode(dynamite)) {
                    int radius = dynamite->item->explosionRadius;
                    createExplosion(game, item->obj->x, item->obj->y, radius);
                    item->free(subItem);
                } else {
                    enqueue(game->items, subItem);
                }
            }
            else if(item->name == mineItem ) {
                Mine *mine = (Mine *) subItem;
                if(mineReadyToExplode(mine)) {
                    int radius = mine->item->explosionRadius;
                    cutCircleInLevel( ((Game *) game)->level, item->obj->x, item->obj->y, radius );
                    Stamp *explosionStamp = createStamp(game->animBank[explosion], 0, false, item->obj->x, item->obj->y);
                    enqueue(game->stamps, explosionStamp);
                    explodeWorms(game->teams, game, item->obj->x, item->obj->y, radius, MAX_JUMP_VELOCITY );
                    item->free(subItem);
                } else {
                    enqueue(game->items, subItem);
                }
            } else {
                enqueue(game->items, subItem);
            }
            
        }

        stampNumber = queueSize(game->stamps);
        for(int i = 0; i < stampNumber; i++) {
            Stamp *stamp = dequeue(game->stamps);
            drawStamp(stamp, stamp->x, stamp->y);
            if(!hasStampFinished(stamp)){
                enqueue(game->stamps, stamp);
            } else {
                clearStamp(stamp, game->level, stamp->x, stamp->y);
                freeStamp(stamp);
            }
        }

        drawWeapon(currentWeapon->name,
                   game->player->obj->x, game->player->obj->y,
                   &weaponFrame, &currentWeapon->lastPlayed, (void *) game, space, game->player->facingRight, weaponDir);

        drawWormName(game, game->player);
        drawInventory(game, game->currentTeam);
        drawTimeLeftInTurn(game);
        drawCrossHair(game, game->player->obj->x, game->player->obj->y, weaponFireDir, weaponForce);
        
        updateScreen(game->screen);
        
        for(int i = 0; i < teamNumber; i++) {
            team = dequeue(game->teams);
            enqueue(game->teams, team);
            wormNumber = team->teamNumber;
            for(int j = 0; j < wormNumber; j++) {
                worm = team->worms[j];
                clearWorm(worm, game->level);
                //clearWormName(game, worm);
            }
        }

        itemNumber = queueSize(game->items);
        for(int i = 0; i < itemNumber; i++) {
            //do item stuff
            void *subItem = dequeue(game->items);
            enqueue(game->items, subItem);
            Item *item = ((ItemSubclass *) subItem)->item;
            clearItem(item, game->level);
        }

        stampNumber = queueSize(game->stamps);
        for(int i = 0; i < stampNumber; i++) {
            Stamp *stamp = dequeue(game->stamps);
            clearStamp(stamp, game->level, stamp->x, stamp->y);
            enqueue(game->stamps, stamp);
        }
        clearWormName(game, game->player);
        clearCrossHair(game, game->player->obj->x, game->player->obj->y, weaponFireDir, weaponForce);
        
        if(timeLeftInTurn(game) <= 0) {
            switchPlayer(game);
            switchTeam(game);
            dropCrates(game, randInt(0, 2));
        }
        game->lastUpdate = clock(); 

        //printf("%d\n", isStill(game));
    }
    
    return false;

}

void readKeys(SDL_Event *event, bool *left, bool *right, bool *up, bool *down, bool *space, bool *tab, bool *esc, bool *enter, bool *backspace, bool *one, bool *two, bool *three, bool *four, bool *five) {
    while(SDL_PollEvent(event)) {
        
        switch (event->type) 
        {
            case SDL_QUIT:
                break;
            case SDL_KEYDOWN:
                switch(event->key.keysym.sym){
                    case SDLK_LEFT:
                        *left = true;
                        break;
                    case SDLK_RIGHT:
                        *right = true;
                        break;
                    case SDLK_UP:
                        *up = true;
                        break;
                    case SDLK_DOWN:
                        *down = true;
                        break;
                    case SDLK_ESCAPE:
                        *esc = true;
                        break;
                    case SDLK_SPACE:
                        *space = true;
                        break;
                    case SDLK_TAB:
                        *tab = true;
                    case SDLK_RETURN:
                        *enter = true;
                        break;
                    case SDLK_BACKSPACE:
                        *backspace = true;
                        break;
                    case SDLK_1:
                        *one = true;
                        break;
                    case SDLK_2:
                        *two = true;
                        break;
                    case SDLK_3:
                        *three = true;
                        break;
                    case SDLK_4:
                        *four = true;
                        break;
                    case SDLK_5:
                        *five = true;
                        break;
                    default:
                        break;
                }
                break;  
            case SDL_KEYUP:
                switch(event->key.keysym.sym) {
                    case SDLK_LEFT:
                        *left = false;
                        break;
                    case SDLK_RIGHT:
                        *right = false;
                        break;
                    case SDLK_UP:
                        *up = false;
                        break;
                    case SDLK_DOWN:
                        *down = false;
                        break;
                    case SDLK_ESCAPE:
                        *esc = false;
                        break;
                    case SDLK_SPACE:
                        *space = false;
                        break;
                    case SDLK_TAB:
                        *tab = false;
                        break;
                    case SDLK_RETURN:
                        *enter = false;
                        break;
                    case SDLK_BACKSPACE:
                        *backspace = false;
                        break;
                    case SDLK_1:
                        *one = false;
                        break;
                    case SDLK_2:
                        *two = false;
                        break;
                    case SDLK_3:
                        *three = false;
                        break;
                    case SDLK_4:
                        *four = false;
                        break;
                    case SDLK_5:
                        *five = false;
                        break;
                    default:
                        break;
                }
                break;
        }
    }
}

void explodeWorms(Queue *teams, Game *game, int x, int y, int radius, float maxVelocity) {
    float newVelocity, dir, strength, distance;
    int teamNumber, wormNumber;
    teamNumber = queueSize(game->teams);
    Team *team;
    Worm *worm;
    for(int i = 0; i < teamNumber; i++) {
        team = dequeue(game->teams);
        enqueue(game->teams, team);
        wormNumber = team->teamNumber;
        for(int j = 0; j < wormNumber; j++) {
            worm = team->worms[j];
            distance = dist(x, y, worm->obj->x, worm->obj->y);
            if(distance <= radius) {
                strength = (radius - distance) / radius;
                hurtWorm(worm, 25 * strength * EXPLOSION_DAMAGE_SCALE);
                newVelocity = strength * maxVelocity;
                //newVelocity = maxVelocity;
                //if worm is too close, direction might not be accurate.
                //compensate by pretending worm is 10 pix higher
                /*
                if(distance < 60){
                    dir = atan2(-1000 + worm->obj->y - y, worm->obj->x - x);
                } else {
                    dir = atan2(worm->obj->y - y, worm->obj->x - x);
                }
                */
                dir = atan2(worm->obj->y - y - 5, worm->obj->x - x);
                //printf("%f \n", distance);
                accel(worm->obj, dir, newVelocity, maxVelocity);
            }
        }
    }
}

void drawInventory(Game *game, Team *team) {
    int cx, cy, f, w, h, aw, ah, ix, iy, weaponNumber, amount;
    WeaponName name;
    InvWeapon *invWeapon;
    f = 0;
    clock_t now = clock();
    char weaponNum[10];
    //width and height of inventory anim
    aw = game->animBank[invAnim]->width;
    ah = game->animBank[invAnim]->height;
    //center of the inventory on the screen
    cx = WIDTH/2;
    cy = HEIGHT - ah / 2;
    //width and height of each cell in the inventory
    w =  aw/ INV_CELLS_WIDE;
    h = ah / INV_CELLS_HIGH;
    playAnim(game->animBank[invAnim], cx, cy, 0, &f, &now, false);
    f = 0;
    playAnim( game->animBank[selectedCell], cx - aw/2 + w/2 + game->currentTeam->selectedWeapon * w + 2, cy - h/2, 0, &f, &now, false);
    weaponNumber = arrayListSize(team->weapons);
    for(int i = 0; i < weaponNumber; i++){
        //coordinates for the cell
        ix = cx - aw/2 + w/2 + i * w;
        iy = cy - h/2;
        f = 0;
        invWeapon = (InvWeapon *) getArrayListElement(team->weapons, i);
        name = invWeapon->weapon.name;
        amount = invWeapon->amount;
        drawWeapon(name, ix, iy, &f, &now, game, false, false, 0);
        sprintf(weaponNum, "%d", amount);
        writeText(game->font, weaponNum, ix, iy + h);
    }
    //writeText(game->font, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.", 10, iy + h);
}

void drawWormName(Game *game, Worm *worm) {
    writeTextWithBackground(game->font, worm->name, worm->obj->x, worm->obj->y - NAME_HEIGHT);
}

void clearWormName(Game *game, Worm *worm) {
    int width, height;
    //Color red = {255, 0, 0};
    width = stringDisplayWidth(game->font, worm->name);
    height = stringDisplayHeight(game->font, worm->name);

    int x,y,w,h;

    x = worm->obj->x - 8;
    y = worm->obj->y - NAME_HEIGHT - height/2 - 8;
    w = width + 16;
    h = height + 16;

    drawLevel(game->level, x / 8, y / 8, x + w , y + h);
    //drawRect(game->screen, x, y, w, h, red);
}

float timeLeftInTurn(Game *game) {
    return game->turnLength - ( (float) (clock() - game->turnStart)) / CLOCKS_PER_SEC;
}

void drawTimeLeftInTurn(Game *game) {
    int timeLeft = timeLeftInTurn(game);
    char timeStr[10];
    sprintf(timeStr, "%.2d", timeLeft);
    int width = stringDisplayWidth(game->font, timeStr);
    writeText(game->font, timeStr, WIDTH / 2 - width / 4, HEIGHT - TIMER_HEIGHT);
    //randomly drop crate
}

bool isStill(Game *game) {
    Team *team;
    float thresh = 1;
    bool isStill = true;
    int wormNumber;
    Worm *worm;
    //WORM
    int teamNumber = queueSize(game->teams);
    for(int i = 0; i < teamNumber; i++) {
        team = dequeue(game->teams);
        enqueue(game->teams, team);
        wormNumber = team->teamNumber;
        for(int j = 0; j < wormNumber; j++) {
            worm = team->worms[j];
            //worm is moving
            if( abs(worm->obj->velocity) >= thresh ) {
                isStill = false;
                break;
            }

        }
    }

    //ITEM
    int itemNumber = queueSize(game->items);
    for(int k = 0; k < itemNumber; k++) {
        void *subItem = dequeue(game->items);
        Item *item = ((ItemSubclass *) subItem)->item;
        enqueue(game->items, subItem);
        //item is moving
        if( abs(item->obj->velocity) >= thresh ) {
            isStill = false;
            break;
        }
        //if there is dynamite it has to explode before the game is calm
        if(item->name == dynamiteItem) {
            isStill = false;
            break;
        }
    }


    return isStill;
}

void switchPlayer(Game *game) {
    game->currentTeam->worms[game->currentTeam->playerIdx]->currentFrame = 0;
    game->currentTeam->worms[game->currentTeam->playerIdx]->currentAnim = game->animBank[wormStill];
    game->currentTeam->playerIdx++;
    game->currentTeam->playerIdx %= game->currentTeam->teamNumber;
}

void switchTeam(Game *game) {
    game->currentTeam = dequeue(game->teams);
    enqueue(game->teams, game->currentTeam);
    game->turnStart = clock();
    game->lastTurn = clock();
    game->waitForTurn = true;
}

void dropCrates(Game *game, int crateNum) {
    for(int i = 0; i < crateNum; i++) {
        enqueue(game->items, createHealthCrate(randInt(0, game->level->width), 0, 60, 100, (void *) game));
        enqueue(game->items, createWeaponCrate(randInt(0, game->level->width), 0, 60, randWeapon((void *) game), randInt(0, 1), (void *) game));
    }
}
void giveWormWeapon(Game *game, Worm *worm, Weapon *weapon) {
    Team *team = (Team *) worm->team;
    InvWeapon *invWeapon;
    int size = arrayListSize(team->weapons);
    for(int i = 0; i < size; i++) {
        invWeapon = ((InvWeapon *) getArrayListElement(team->weapons, i));
        if(invWeapon->weapon.name == weapon->name) {
            invWeapon->amount += randInt(1, WEAPON_AMOUNT_MAX);
            freeWeapon(weapon);
            return;
        }
    }
    invWeapon = (InvWeapon *) malloc(sizeof(InvWeapon));
    invWeapon->weapon = *weapon;
    invWeapon->amount = randInt(1, WEAPON_AMOUNT_MAX);
    addToArrayListEnd(team->weapons, invWeapon);
}

void setGameSelectedWeapon(Game *game, int weaponIdx) {
    if(weaponIdx < arrayListSize(game->currentTeam->weapons)) {
        game->currentTeam->selectedWeapon = weaponIdx;
    }
}

void createExplosion(Game *game, int x, int y, int r) {
    printf("Explosion: %d\n", r);
    cutCircleInLevel(game->level, x, y, r);
    Stamp *explosionStamp;
    if(r <= BIG_EXPLOSION_SIZE) {
        explosionStamp = createStamp(game->animBank[explosion], 0, false, x, y);
    } else {
        explosionStamp = createStamp(game->animBank[bigExplosion], 0, false, x, y);
    }
    enqueue(game->stamps, explosionStamp);
    explodeWorms(game->teams, game, x, y, r, MAX_JUMP_VELOCITY );
}

bool wormOffGrid(Worm *worm) {
    return worm->obj->x < -DEATH_BORDER || worm->obj->x > WIDTH + DEATH_BORDER || worm->obj->y > HEIGHT;
}

void clearKeys(bool *left, bool *right, bool *up,
              bool *down, bool *space, bool *tab, bool *esc, bool *enter,
              bool *backspace, bool *one, bool *two, bool *three, bool *four, bool *five) {
                  *left = false;
                  *right = false;
                  *up = false;
                  *down = false;
                  *space = false;
                  *tab = false;
                  *esc = false;
                  *enter = false;
                  *backspace = false;
                  *one = false;
                  *two = false;
                  *three = false;
                  *four = false;
                  *five = false;
              }